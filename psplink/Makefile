TARGET = psplink
OBJS = main.o shell.o config.o bitmap.o tty.o decodeaddr.o memoryUID.o kmode.o exception.o exception_asm.o psplinkcnf.o util.o debug.o libs.o apihook.o thctx.o stdio.o usbshell.o modload.o exports.o

# Use only kernel libraries
USE_KERNEL_LIBS = 1

INCDIR = ../usbhostfs
CFLAGS = -Os -mno-gpopt -Wall -fno-builtin-printf -ggdb
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LDFLAGS=-nodefaultlibs -L../libusbhostfs_driver

LIBDIR =

LIBS = -lpspusb_driver -lpspusbstor -lpspumd -lusbhostfs_driver

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

LIBS += -lpsppower_driver -lpspdisplay_driver
