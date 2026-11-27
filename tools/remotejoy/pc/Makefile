WERROR ?= $(if $(filter true 1,$(CI)),1,0)

OUTPUT=remotejoy
OBJS=remotejoy.o

CFLAGS=-Wall $(if $(filter 1,$(WERROR)),-Werror) -g

PREFIX=$(shell psp-config --pspdev-path 2> /dev/null)

all: $(OUTPUT)

include ../../../pspsdk-headers.mk

$(OUTPUT): $(OBJS)
	$(CC) -o $@ $^ 

remotejoy.o: override CPPFLAGS += $(PSPCTRL_CPPFLAGS)
remotejoy.o: ../remotejoy.h $(PSPCTRL_HEADER)

install: $(OUTPUT)
	@echo "Installing $(OUTPUT)..."
	@if ( test $(PREFIX) ); then { mkdir -p $(PREFIX)/bin && cp $(OUTPUT) $(PREFIX)/bin; } else { echo "Error: psp-config not found!"; exit 1; } fi
	@echo "Done!"

clean:
	rm -f $(OUTPUT) *.o
