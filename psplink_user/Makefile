TARGET = psplink_user
OBJS = main.o kmode.o

BUILD_PRX=1
PRX_EXPORTS=exports.exp

INCDIR = 
CFLAGS = -O2 -G0 -Wall -I../libpsplink
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
LDFLAGS= -mno-crt0 -nostartfiles -L../libpsplink
ASFLAGS = $(CFLAGS)

LIBDIR =

LIBS = -lpsplink

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
