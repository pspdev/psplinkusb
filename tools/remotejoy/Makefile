TARGET = remotejoy
OBJS = main.o kmode.o exports.o

# Use only kernel libraries
USE_KERNEL_LIBS = 1

INCDIR = 
CFLAGS = -Os -mno-gpopt -Wall -fno-builtin-printf -I../../usbhostfs -I../../psplink
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LDFLAGS = -L../../libpsplink_driver -L../../libusbhostfs_driver
LIBS = -lpsppower_driver -lusbhostfs_driver -lpspdisplay_driver

ifdef BUILD_PLUGIN
CFLAGS += -DBUILD_PLUGIN
#OBJS += libs.o apihook.o
LIBS += -lpspusb_driver
else
LIBS += -lpsplink_driver
endif

LIBDIR =


PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
