WERROR ?= $(if $(filter true 1,$(CI)),1,0)

TARGET = webget
OBJS = webget.o

BUILD_PRX=1

INCDIR = 
CFLAGS = -O2 -mno-gpopt -Wall $(if $(filter 1,$(WERROR)),-Werror)
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS=-lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
