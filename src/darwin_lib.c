#include <libusb.h>
#include "libusbgetdev.h"
#include "libusbgetdevi.h"

#include <IOKit/IOTypes.h>
#include <IOKit/IOCFBundle.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOBSD.h>
#include <IOKit/serial/IOSerialKeys.h>

#if !defined(kIOUSBHostInterfaceClassName)
#define kIOUSBHostInterfaceClassName "IOUSBHostInterface"
#endif

#if !defined(kUSBHostMatchingPropertyInterfaceNumber)
#define kUSBHostMatchingPropertyInterfaceNumber "bInterfaceNumber"
#endif

#if !defined(IO_OBJECT_NULL)
#define IO_OBJECT_NULL ((io_object_t) 0)
#endif

/* Both kIOMasterPortDefault or kIOMainPortDefault are synonyms for 0. */
static const mach_port_t darwin_default_master_port = 0;

static io_service_t usb_find_interface_matching_location (const io_name_t class_name, UInt8 interface_number, UInt32 location) {
	CFMutableDictionaryRef matchingDict = IOServiceMatching (class_name);
	CFMutableDictionaryRef propertyMatchDict = CFDictionaryCreateMutable (kCFAllocatorDefault, 0,
									      &kCFTypeDictionaryKeyCallBacks,
									      &kCFTypeDictionaryValueCallBacks);
	CFTypeRef locationCF = CFNumberCreate (NULL, kCFNumberSInt32Type, &location);
	CFTypeRef interfaceCF = CFNumberCreate (NULL, kCFNumberSInt8Type, &interface_number);

	CFDictionarySetValue (matchingDict, CFSTR(kIOPropertyMatchKey), propertyMatchDict);
	CFDictionarySetValue (propertyMatchDict, CFSTR(kUSBDevicePropertyLocationID), locationCF);
	CFDictionarySetValue (propertyMatchDict, CFSTR(kUSBHostMatchingPropertyInterfaceNumber), interfaceCF);

	CFRelease (interfaceCF);
	CFRelease (locationCF);
	CFRelease (propertyMatchDict);

	return IOServiceGetMatchingService (darwin_default_master_port, matchingDict);
}

static IOReturn IORegistryChildConformsTo(const io_service_t *service, io_service_t *child, const io_name_t class_name) {
	io_service_t	int_service = IO_OBJECT_NULL;
	io_iterator_t	deviceIterator = MACH_PORT_NULL;
	IOReturn	kresult;

	*child = IO_OBJECT_NULL;

	kresult = IORegistryEntryCreateIterator (*service, kIOServicePlane, kIORegistryIterateRecursively, &deviceIterator);
	if (kresult != kIOReturnSuccess)
		return kresult;

	while (true) {
		int_service = IOIteratorNext (deviceIterator);
		if (int_service == IO_OBJECT_NULL)
			break;

		if (IOObjectConformsTo (int_service, class_name) == TRUE) {
			*child = int_service;
			IOObjectRelease (deviceIterator);

			return kIOReturnSuccess;
		}

		IOObjectRelease (int_service);
	}

	IOObjectRelease (deviceIterator);

	return kIOReturnNoDevice;
}

#ifdef HAVE_PLAT_DEVID
static int darwin_get_location(struct libusb_device *dev, UInt32 *location) {
	CFMutableDictionaryRef	matchingDict;
	unsigned long long int	loc;
	io_service_t		service = IO_OBJECT_NULL;
	CFTypeRef		cf_location;
	UInt32			ret_loc;
	bool			success;
	char			*path;
	int			ret;

	ret = libusb_get_platform_device_id(dev, &path);
	if (ret != LIBUSB_SUCCESS) {
		return ret;
	}

	loc = strtoull(path, NULL, 10);
	free(path);

	if (loc == 0 || loc > UINT64_MAX)
		return LIBUSB_ERROR_INVALID_PARAM;

	matchingDict = IORegistryEntryIDMatching(loc);

	service = IOServiceGetMatchingService (darwin_default_master_port, matchingDict);
	cf_location = IORegistryEntryCreateCFProperty (service, CFSTR("locationID"), kCFAllocatorDefault, 0);

	if (!cf_location) {
		IOObjectRelease (service);
		return LIBUSB_ERROR_INVALID_PARAM;
	}

	success = CFNumberGetValue (cf_location, kCFNumberSInt32Type, &ret_loc);
	CFRelease (cf_location);
	IOObjectRelease (service);

	if (!success)
		return LIBUSB_ERROR_INVALID_PARAM;

	*location = ret_loc;

	return LIBUSB_SUCCESS;
}
#else
static int darwin_get_location(struct libusb_device *dev, UInt32 *location) {
	(void)dev;

	*location = 0;
	return LIBUSB_ERROR_NOT_FOUND;
}
#endif

int get_dev_path(struct libusb_device *dev, int iface_idx,
	enum usbi_dev_type dev_type, char **path) {
	static const char	char_prefix[] = "/dev/tty.";
	static const char	blk_prefix[] = "/dev/";
	io_service_t		service = IO_OBJECT_NULL;
	io_service_t		child = IO_OBJECT_NULL;
	IOReturn		kresult;
	CFTypeRef		cf_bsd;
	UInt32			location;
	bool			ret;
	int			iret;
	CFIndex			length;

/*
	if (0 == priv->active_config)
		return LIBUSB_ERROR_NOT_FOUND;
*/

	iret = darwin_get_location (dev, &location);
	if (iret != LIBUSB_SUCCESS)
		return iret;

	service = usb_find_interface_matching_location (kIOUSBHostInterfaceClassName, iface_idx, location);

	if (dev_type == USBI_DEV_BLOCK)
		kresult = IORegistryChildConformsTo (&service, &child, "IOStorage");
	else if (dev_type == USBI_DEV_CHAR)
		kresult = IORegistryChildConformsTo (&service, &child, "IOSerialBSDClient");
	else
		kresult = kIOReturnNoDevice; // Returns LIBUSB_ERROR_NOT_FOUND

	IOObjectRelease (service);

	if (kresult != kIOReturnSuccess)
		return LIBUSB_ERROR_NOT_FOUND;

	if (dev_type == USBI_DEV_BLOCK)
		cf_bsd = IORegistryEntrySearchCFProperty (child, kIOServicePlane, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, kIORegistryIterateRecursively);
	else
		cf_bsd = IORegistryEntrySearchCFProperty (child, kIOServicePlane, CFSTR(kIOTTYDeviceKey), kCFAllocatorDefault, kIORegistryIterateRecursively);
	if (!cf_bsd) {
		IOObjectRelease (child);
		return LIBUSB_ERROR_NOT_FOUND;
	}

	/* Add 1 for NULL terminator */
	length = CFDataGetLength (cf_bsd) + 1;

	if (dev_type == USBI_DEV_BLOCK)
		*path = malloc (length + (sizeof(blk_prefix) - 1));
	else
		*path = malloc (length + (sizeof(char_prefix) - 1));

	if (!*path)
		return LIBUSB_ERROR_NO_MEM;

	if (dev_type == USBI_DEV_BLOCK) {
		memcpy (*path, blk_prefix, sizeof(blk_prefix) - 1);
		ret = CFStringGetCString (cf_bsd, *path + (sizeof(blk_prefix) - 1), length, kCFStringEncodingASCII);
	} else {
		memcpy (*path, char_prefix, sizeof(char_prefix) - 1);
		ret = CFStringGetCString (cf_bsd, *path + (sizeof(char_prefix) - 1), length, kCFStringEncodingASCII);
	}

	CFRelease (cf_bsd);
	IOObjectRelease (child);

	if (ret)
		return LIBUSB_SUCCESS;

	free (*path);
	*path = NULL;

	return LIBUSB_ERROR_NOT_FOUND;
}
