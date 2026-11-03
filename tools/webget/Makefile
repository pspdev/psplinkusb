TARGET = webget
OBJS = webget.o

BUILD_PRX=1

INCDIR = 
CFLAGS = -O2 -mno-gpopt -Wall -Werror
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS=-lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
