OUTPUT=usbhostfs_pc
OBJS=main.o
LIBS=-lpthread $(shell pkg-config --libs libusb-1.0)
CFLAGS=-Wall -ggdb -I../usbhostfs -DPC_SIDE -D_FILE_OFFSET_BITS=64 -I. -O2 $(shell pkg-config --cflags libusb-1.0)
LDFLAGS=

PREFIX=$(shell psp-config --pspdev-path 2> /dev/null)

ifdef BUILD_BIGENDIAN
CFLAGS += -DBUILD_BIGENDIAN
endif

ifdef NO_UID_CHECK
CFLAGS += -DNO_UID_CHECK
endif

ifdef READLINE_SHELL
CFLAGS += -DREADLINE_SHELL
LIBS += -lreadline
endif

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(LINK.c) $(LDFLAGS) -o $@ $^ $(LIBS)

install: $(OUTPUT)
	@echo "Installing $(OUTPUT)..."
	@if ( test $(PREFIX) ); then { mkdir -p $(PREFIX)/bin && cp $(OUTPUT) $(PREFIX)/bin; } else { echo "Error: psp-config not found!"; exit 1; } fi
	@if ( test $(PREFIX) ); then { mkdir -p $(PREFIX)/share/psplinkusb && cp 50-psplink.rules $(PREFIX)/share/psplinkusb; } else { echo "Error: psp-config not found!"; exit 1; } fi
	@echo "Done!"

clean:
	rm -f $(OUTPUT) *.o
