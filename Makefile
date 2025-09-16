BUILD_DIR = build
LIBRARY = libusbgetdev
PROGRAM = listdevs
URL = https://github.com/Mr-Bossman/libusbgetdev
VERSION = 0.1.0

CFLAGS :=	-Wall \
		-Wextra \
		-fPIC

LDFLAGS :=

PREFIX ?= /usr/local
PKG_CONFIG ?= pkg-config

HOST := $(shell $(CC) -dumpmachine)
C_SOURCES := src/libusbgetdev.c
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
DEPS = $(OBJECTS:%.o=%.d)

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.o $(BUILD_DIR)

ifneq (, $(shell $(PKG_CONFIG) --version 2>/dev/null))
	CFLAGS += $(shell $(PKG_CONFIG) --cflags libusb-1.0)
	LDFLAGS += $(shell $(PKG_CONFIG) --libs libusb-1.0)
else
	CFLAGS += -I/usr/include/libusb-1.0
	LDFLAGS += -lusb-1.0
endif

run_check = printf "$(1)" | $(CC) -x c -c -S -Wall -Werror $(CFLAGS) -o - - > /dev/null 2>&1; echo $$?

have_plat_devid=\#include <libusb.h>\n int main(void) { return sizeof(libusb_get_platform_device_id); }

ifeq (0, $(shell $(call run_check,$(have_plat_devid))))
CFLAGS += -DHAVE_PLAT_DEVID
endif

ifneq (, $(findstring linux, $(HOST)))
C_SOURCES += src/linux_lib.c
SONAME = $(LIBRARY).so
LDCONFIG ?= ldconfig
else ifneq (, $(findstring darwin, $(HOST)))
C_SOURCES += src/darwin_lib.c
SONAME = $(LIBRARY).dylib
LDFLAGS += -framework IOKit -framework CoreFoundation -dynamiclib
SET_SONAME = install_name_tool -id $(PREFIX)/lib/$(SONAME) $(BUILD_DIR)/$(SONAME)
else ifneq (, $(findstring msys, $(HOST)))
PREFIX := /usr
C_SOURCES += src/windows_lib.c
SONAME = $(LIBRARY).dll
LDFLAGS += -lsetupapi
LIBS = -lsetupapi
else
$(error 'Could not determine the host type. Please set the $$HOST variable.')
endif

define newline


endef
_file = @printf "$(subst $(newline),\n,$(1))" $(2) "$(3)"

define PKG_CONFIG_FILE
prefix=$(PREFIX)
exec_prefix=\$${prefix}
includedir=\$${prefix}/include
libdir=\$${exec_prefix}/lib

Name: $(LIBRARY)
Description: A cross-platform library to correlate USB devices to block or character devices
URL: $(URL)
Version: $(VERSION)
Requires: libusb-1.0
Cflags: -I\$${includedir}/$(LIBRARY)
Libs: -L\$${libdir} -l$(LIBRARY:lib%=%) $(LIBS)

endef

.PHONY: all clean debug example

all: $(BUILD_DIR)/$(SONAME) $(BUILD_DIR)/$(LIBRARY).a $(BUILD_DIR)/$(LIBRARY).pc

debug: CFLAGS += -g
debug: all

example:
	@$(MAKE) $(PROGRAM) C_SOURCES="$(C_SOURCES) src/listdevs.c"

$(OBJECTS): | $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $@

-include $(DEPS)
$(BUILD_DIR)/%.o: %.c
	$(CC) -MMD -c $(CFLAGS) $< -o $@

$(PROGRAM): $(OBJECTS)
	$(CC) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/$(SONAME): $(OBJECTS)
	$(CC) -shared $^ $(LDFLAGS) -o $@
	$(SET_SONAME)

$(BUILD_DIR)/$(LIBRARY).a: $(OBJECTS)
	$(AR) rcs $@ $^


$(BUILD_DIR)/$(LIBRARY).pc: $(BUILD_DIR) Makefile
	@echo "Generating $@"
	$(call _file,$(PKG_CONFIG_FILE),>,$@)

install: all
	install -m 755 -d $(PREFIX)/include/$(LIBRARY)
	install -m 644 src/$(LIBRARY).h $(PREFIX)/include/$(LIBRARY)
	install -m 644 $(BUILD_DIR)/$(SONAME) $(PREFIX)/lib
	install -m 644 $(BUILD_DIR)/$(LIBRARY).a $(PREFIX)/lib
	install -m 644 $(BUILD_DIR)/$(LIBRARY).pc $(PREFIX)/lib/pkgconfig
	$(LDCONFIG)

uninstall:
	-rm -rf $(PREFIX)/include/$(LIBRARY)
	-rm -f $(PREFIX)/lib/pkgconfig/$(LIBRARY).pc
	-rm -f $(PREFIX)/lib/$(SONAME)
	-rm -f $(PREFIX)/lib/$(LIBRARY).a

clean:
	-rm -rf $(BUILD_DIR)
	-rm -f $(PROGRAM)
