#ifndef LIBUSBGETDEVI_H
#define LIBUSBGETDEVI_H

#include <stdlib.h>
#include <libusb.h>
#include "libusbgetdev.h"

enum usbi_dev_type {
	USBI_DEV_BLOCK = 1,
	USBI_DEV_CHAR = 2,
};

int get_dev_path(struct libusb_device *dev, int iface_idx,
	enum usbi_dev_type dev_type, char **path);

#endif /* !LIBUSBGETDEVI_H */
