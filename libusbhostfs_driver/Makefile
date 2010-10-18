TARGET=libusbhostfs_driver.a
all: $(TARGET)
OBJS = USBHostFS_driver_0000.o USBHostFS_driver_0001.o USBHostFS_driver_0002.o USBHostFS_driver_0003.o USBHostFS_driver_0004.o USBHostFS_driver_0005.o USBHostFS_driver_0006.o USBHostFS_driver_0007.o USBHostFS_driver_0008.o USBHostFS_driver_0009.o USBHostFS_driver_0010.o 

PSPSDK=$(shell psp-config --pspsdk-path)

clean:
	rm -f $(OBJS) $(TARGET)

CC=psp-gcc
INCDIR = 
CFLAGS = -Os -G0 -Wall -fno-builtin-printf -I$(PSPSDK)/include
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LDFLAGS=-nodefaultlibs

LIBDIR =

$(TARGET): $(OBJS)
	psp-ar q $(TARGET) $(OBJS)
	psp-ranlib $(TARGET)


$(OBJS): USBHostFS_driver.S
	$(CC) $(CFLAGS) -DF_$* $< -c -o $@

