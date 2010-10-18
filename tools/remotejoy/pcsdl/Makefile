OUTPUT=remotejoy
OBJS=remotejoy.o font.o

CFLAGS=-O2 -Wall -g $(shell sdl-config --cflags)

PREFIX=$(shell psp-config --pspdev-path 2> /dev/null)

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(CC) -o $@ $^ $(shell sdl-config --libs)

install: $(OUTPUT)
	@echo "Installing $(OUTPUT)..."
	@if ( test $(PREFIX) ); then { mkdir -p $(PREFIX)/bin && cp $(OUTPUT) $(PREFIX)/bin; } else { echo "Error: psp-config not found!"; exit 1; } fi
	@echo "Done!"

clean:
	rm -f $(OUTPUT) *.o
