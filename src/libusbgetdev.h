#ifndef LIBUSBGETDEV_H
#define LIBUSBGETDEV_H

#include <stdlib.h>
#include <libusb.h>

int libusb_get_blockdev_path(libusb_device *dev, int iface_idx, char **path);
int libusb_get_chardev_path(libusb_device *dev, int iface_idx, char **path);

#endif /* !LIBUSBGETDEV_H */
