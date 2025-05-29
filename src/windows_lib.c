#include <initguid.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <Devpkey.h>

#include <libusb.h>
#include "libusbgetdev.h"
#include "libusbgetdevi.h"

static int match_dev_path(enum usbi_dev_type dev_type, const char *dev_reg_path, char **path);
static int get_parent(HDEVINFO device_info_set, PSP_DEVINFO_DATA device_info_data, char** parent);
static int get_chardev(HDEVINFO device_info_set, PSP_DEVINFO_DATA device_info_data, char** com);
static int get_blockdev(HDEVINFO device_info_set, PSP_DEVICE_INTERFACE_DATA device_interface_data, char **path);

#ifdef HAVE_PLAT_DEVID
static int get_devid(struct libusb_device *dev, char **DeviceID)
{
	return libusb_get_platform_device_id(dev, DeviceID);
}
#else
static int get_devid(struct libusb_device *dev, char **DeviceID)
{
	(void)dev;
	(void)DeviceID;

	return LIBUSB_ERROR_NOT_FOUND;
}
#endif

int get_dev_path(struct libusb_device *dev, int iface_idx,
	enum usbi_dev_type dev_type, char **path)
{
	char *DeviceID;

	(void)iface_idx;

	if (get_devid(dev, &DeviceID) != LIBUSB_SUCCESS)
		return LIBUSB_ERROR_NOT_FOUND;

	return match_dev_path(dev_type, DeviceID, path);
}

static int match_dev_path(enum usbi_dev_type dev_type, const char *DeviceID, char **path) {
	HDEVINFO device_info_set;
	SP_DEVINFO_DATA device_info_data;
	SP_DEVICE_INTERFACE_DATA device_interface_data;
	DWORD deviceIndex = 0;
	const GUID *ClassGuid;
	char* parent;
	int ret;

	*path = NULL;

	if (dev_type == USBI_DEV_BLOCK)
		ClassGuid = (const GUID *) &GUID_DEVINTERFACE_DISK;
	else if (dev_type == USBI_DEV_CHAR)
		ClassGuid = (const GUID *) &GUID_DEVCLASS_PORTS;
	else
		return -1;

	device_info_set = SetupDiGetClassDevs(ClassGuid, NULL, NULL,
					      DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
	device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	while (SetupDiEnumDeviceInfo(device_info_set, deviceIndex, &device_info_data)) {

		SetupDiEnumDeviceInterfaces(device_info_set, NULL, ClassGuid,
					    deviceIndex, &device_interface_data);

		get_parent(device_info_set, &device_info_data, &parent);

		if (strcmp(parent, DeviceID) != 0) {
			free(parent);
			deviceIndex++;
			continue;
		}

		free(parent);

		if (dev_type == USBI_DEV_BLOCK)
			ret = get_blockdev(device_info_set, &device_interface_data, path);
		else
			ret = get_chardev(device_info_set, &device_info_data, path);

		if (!ret)
			break;

		deviceIndex++;
	}

	SetupDiDestroyDeviceInfoList(device_info_set);

	return ret;

}

static int get_parent(HDEVINFO device_info_set, PSP_DEVINFO_DATA device_info_data, char** parent) {
	DEVPROPTYPE property_type;
	DWORD required_size;
	LPWSTR wpath;
	size_t path_size;

	SetupDiGetDevicePropertyW(device_info_set, device_info_data, &DEVPKEY_Device_Parent, &property_type, NULL, 0, &required_size, 0);

	wpath = (LPWSTR)malloc(required_size);
	SetupDiGetDevicePropertyW(device_info_set, device_info_data, &DEVPKEY_Device_Parent, &property_type, (PBYTE)wpath, required_size, NULL, 0);

	path_size = WideCharToMultiByte(CP_UTF8, 0, wpath, required_size / sizeof(wchar_t), NULL, 0, NULL, NULL);

	*parent = (char*)malloc(path_size + 1);
	WideCharToMultiByte(CP_UTF8, 0, wpath, required_size / sizeof(wchar_t), *parent, path_size, NULL, NULL);

	free(wpath);
	return 0;
}

static int get_chardev(HDEVINFO device_info_set, PSP_DEVINFO_DATA device_info_data, char** path) {
	HKEY hkey;
	LSTATUS return_code;
	DWORD port_name_length;

	hkey = SetupDiOpenDevRegKey(device_info_set, device_info_data, DICS_FLAG_GLOBAL,
				    0, DIREG_DEV, KEY_READ);

	return_code = RegQueryValueEx(hkey, "PortName",
				      NULL, NULL, NULL,
				      &port_name_length);

	if (return_code != EXIT_SUCCESS) {
		RegCloseKey(hkey);
		return -1;
	}

	*path = (char *)malloc(port_name_length + 1);

	return_code = RegQueryValueEx(hkey, "PortName",
				      NULL, NULL,
				      (LPBYTE)(*path),
				      &port_name_length);

	(*path)[port_name_length] = '\0';

	RegCloseKey(hkey);

	if (return_code != EXIT_SUCCESS) {
		free(*path);
		*path = NULL;
		return -1;
	}

	// Ignore parallel ports
	if (strstr(*path, "LPT") != NULL) {
		free(*path);
		*path = NULL;
		return -1;
	}

	return 0;
}

static int get_blockdev(HDEVINFO device_info_set, PSP_DEVICE_INTERFACE_DATA device_interface_data,char** path) {
	PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData;
	DWORD requiredSize;

	HANDLE disk = INVALID_HANDLE_VALUE;
	STORAGE_DEVICE_NUMBER diskNumber;
	DWORD bytesReturned;
	size_t path_size;

	SetupDiGetDeviceInterfaceDetail(device_info_set,
					device_interface_data,
					NULL, 0,
					&requiredSize, NULL);

	deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);

	deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

	SetupDiGetDeviceInterfaceDetail(device_info_set,
					device_interface_data,
					deviceInterfaceDetailData,
					requiredSize, NULL, NULL);

	disk = CreateFile(deviceInterfaceDetailData->DevicePath,
			  GENERIC_READ,
			  FILE_SHARE_READ | FILE_SHARE_WRITE,
			  NULL,
			  OPEN_EXISTING,
			  FILE_ATTRIBUTE_NORMAL,
			  NULL);

	DeviceIoControl(disk, IOCTL_STORAGE_GET_DEVICE_NUMBER,
			NULL, 0, &diskNumber, sizeof(STORAGE_DEVICE_NUMBER),
			&bytesReturned, NULL);

	CloseHandle(disk);
	disk = INVALID_HANDLE_VALUE;

	path_size = snprintf(NULL, 0, "\\\\.\\PhysicalDrive%d", diskNumber.DeviceNumber) + 1;
	*path = (char*)malloc(path_size);
	sprintf(*path, "\\\\.\\PhysicalDrive%d", diskNumber.DeviceNumber);
	free(deviceInterfaceDetailData);

	return 0;
}
