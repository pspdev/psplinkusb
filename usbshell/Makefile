TARGET = usbshell
OBJS = main.o exports.o

# Use only kernel libraries
USE_KERNEL_LIBS = 1

INCDIR = 
CFLAGS = -Os -mno-gpopt -Wall -fno-builtin-printf -I../usbhostfs
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LDFLAGS = -L../libpsplink_driver -L../libusbhostfs_driver

LIBDIR =

LIBS = -lpsplink_driver -lusbhostfs_driver

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
