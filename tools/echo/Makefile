TARGET = echo
OBJS = main.o

BUILD_PRX=1

INCDIR = 
CFLAGS = -O2 -mno-gpopt -Wall -I../../usbhostfs
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR = 
LDFLAGS = -L../../libusbhostfs

LIBS = -lusbhostfs

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
