TARGET = scrkprintf
OBJS = main.o 

# Use only kernel libraries
USE_KERNEL_LIBS = 1

INCDIR = 
CFLAGS = -Os -mno-gpopt -Wall -fno-builtin-printf
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =

LIBS = -lpspdebug -lpspge_driver

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
