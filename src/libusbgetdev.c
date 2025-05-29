#include <stdio.h>

#include <libusb.h>
#include "libusbgetdev.h"
#include "libusbgetdevi.h"

/** \ingroup libusb_misc
 * Get the block device path of USB resource.
 * A string that contains a block device that is associated
 * with the USB resource using the active config descriptor.
 * An example path on *nix is `/dev/sdaX`
 *
 * \param dev a device
 * \param iface_idx the <tt>bInterfaceNumber</tt> of the interface you wish to probe
 * \param path pointer to an allocated string that will contain the block device path
 * if the function is successful, or NULL on error
 * \note The caller is responsible for freeing the string.
 * \returns 0 on success
 * \returns \ref LIBUSB_ERROR_NOT_FOUND the device doesn't have an associated device
 * \returns another LIBUSB_ERROR code on error
 */
int libusb_get_blockdev_path(libusb_device *dev, int iface_idx, char **path)
{
	struct libusb_config_descriptor *config;
	int r;

	*path = NULL;

	r = libusb_get_active_config_descriptor(dev, &config);
	if (r < 0) {
		fprintf(stderr, "could not retrieve active config descriptor");
		return LIBUSB_ERROR_OTHER;
	}

	if (iface_idx >= config->bNumInterfaces)
		return LIBUSB_ERROR_NOT_FOUND;

	return get_dev_path(dev, iface_idx, USBI_DEV_BLOCK, path);
}

/** \ingroup libusb_misc
 * Get the character device path of USB resource.
 * A string that contains a character device that is associated
 * with the USB resource using the active config descriptor.
 * An example path on *nix is `/dev/ttyX`
 *
 * \param dev a device
 * \param iface_idx the <tt>bInterfaceNumber</tt> of the interface you wish to probe
 * \param path pointer to an allocated string that will contain the character device path
 * if the function is successful, or NULL on error
 * \note The caller is responsible for freeing the string.
 * \returns 0 on success
 * \returns \ref LIBUSB_ERROR_NOT_FOUND the device doesn't have an associated device
 * \returns another LIBUSB_ERROR code on error
 */
int libusb_get_chardev_path(libusb_device *dev, int iface_idx, char **path)
{
	struct libusb_config_descriptor *config;
	int r;

	*path = NULL;

	r = libusb_get_active_config_descriptor(dev, &config);
	if (r < 0) {
		fprintf(stderr, "could not retrieve active config descriptor");
		return LIBUSB_ERROR_OTHER;
	}

	if (iface_idx >= config->bNumInterfaces)
		return LIBUSB_ERROR_NOT_FOUND;

	return get_dev_path(dev, iface_idx, USBI_DEV_CHAR, path);
}
