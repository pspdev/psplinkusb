TARGET=libpsplink.a
all: $(TARGET)
OBJS = psplink_0000.o psplink_0001.o psplink_0002.o psplink_0003.o psplink_0004.o psplink_0005.o psplink_0006.o psplink_0007.o psplink_0008.o psplink_0009.o psplink_0010.o 

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


$(OBJS): psplink.S
	$(CC) $(CFLAGS) -DF_$* $< -c -o $@

