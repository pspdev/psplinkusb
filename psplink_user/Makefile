TARGET = psplink_user
OBJS = main.o kmode.o

BUILD_PRX=1
PRX_EXPORTS=exports.exp

INCDIR = 
CFLAGS = -O2 -mno-gpopt -Wall -I../libpsplink
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
LDFLAGS= -nostartfiles -L../libpsplink
ASFLAGS = $(CFLAGS)

LIBDIR =

LIBS = -lpsplink

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
