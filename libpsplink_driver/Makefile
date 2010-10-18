TARGET=libpsplink_driver.a
all: $(TARGET)
OBJS = psplink_driver_0000.o psplink_driver_0001.o psplink_driver_0002.o psplink_driver_0003.o psplink_driver_0004.o psplink_driver_0005.o psplink_driver_0006.o psplink_driver_0007.o psplink_driver_0008.o psplink_driver_0009.o psplink_driver_0010.o psplink_driver_0011.o psplink_driver_0012.o psplink_driver_0013.o psplink_driver_0014.o psplink_driver_0015.o psplink_driver_0016.o psplink_driver_0017.o psplink_driver_0018.o 

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


$(OBJS): psplink_driver.S
	$(CC) $(CFLAGS) -DF_$* $< -c -o $@

