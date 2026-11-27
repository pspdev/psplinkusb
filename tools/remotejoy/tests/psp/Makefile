WERROR ?= $(if $(filter true 1,$(CI)),1,0)

TARGET = remotejoy_test
OBJS = main.o
BUILD_PRX = 1
CFLAGS = -O2 -Wall $(if $(filter 1,$(WERROR)),-Werror)

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
