TARGET = debugmenu
OBJS = main.o exports.o

# Use only kernel libraries
USE_KERNEL_LIBS = 1

INCDIR = 
LDFLAGS = -L../../libpsplink
CFLAGS = -Os -mno-gpopt -Wall -fno-builtin-printf -I../../usbhostfs -I../../psplink
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LIBS = -lpsplink


PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

LIBS += -lpspge_driver -lpsppower_driver
