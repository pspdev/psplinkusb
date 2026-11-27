WERROR ?= $(if $(filter true 1,$(CI)),1,0)

OUTPUT=pspsh
OBJS=pspsh.o joy.o parse_args.o pspkerror.o asm.o disasm.o

ifeq ($(DEBUG), 1)
   CXXFLAGS += -O0 -g
else
   CXXFLAGS	+= -O3
endif

CXXFLAGS+= -Wall $(if $(filter 1,$(WERROR)),-Werror) -D_PCTERM -I../psplink
LIBS=-lreadline

PREFIX=$(shell psp-config --pspdev-path 2> /dev/null)

all: $(OUTPUT)

include ../pspsdk-headers.mk

$(OUTPUT): $(OBJS)
	$(CXX) -o $@ $^ $(LIBS)

joy.o: override CPPFLAGS += $(PSPCTRL_CPPFLAGS)
joy.o: joy.h ../tools/remotejoy/remotejoy.h $(PSPCTRL_HEADER)
pspsh.o: joy.h ../psplink/shellcmd.h ../usbhostfs/usbasync.h

install: $(OUTPUT)
	@echo "Installing $(OUTPUT)..."
	@if ( test $(PREFIX) ); then { mkdir -p $(PREFIX)/bin && cp $(OUTPUT) $(PREFIX)/bin; } else { echo "Error: psp-config not found!"; exit 1; } fi
	@echo "Done!"

clean:
	rm -f $(OUTPUT) *.o
