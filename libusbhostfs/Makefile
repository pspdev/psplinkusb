TARGET=libusbhostfs.a
all: $(TARGET)
OBJS = USBHostFS_0000.o USBHostFS_0001.o USBHostFS_0002.o USBHostFS_0003.o USBHostFS_0004.o USBHostFS_0005.o USBHostFS_0006.o USBHostFS_0007.o USBHostFS_0008.o USBHostFS_0009.o USBHostFS_0010.o 

PSPSDK=$(shell psp-config --pspsdk-path)

clean:
	rm -f $(OBJS) $(TARGET)

CC=psp-gcc
INCDIR = 
CFLAGS = -Os -mno-gpopt -Wall -fno-builtin-printf -I$(PSPSDK)/include
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LDFLAGS=-nodefaultlibs

LIBDIR =

$(TARGET): $(OBJS)
	psp-ar q $(TARGET) $(OBJS)
	psp-ranlib $(TARGET)


$(OBJS): USBHostFS.S
	$(CC) $(CFLAGS) -DF_$* $< -c -o $@

