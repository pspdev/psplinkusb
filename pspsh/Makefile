OUTPUT=pspsh
OBJS=pspsh.o parse_args.o pspkerror.o asm.o disasm.o

ifeq ($(DEBUG), 1)
   CXXFLAGS += -O0 -g
else
   CXXFLAGS	+= -O3
endif

CXXFLAGS+= -Wall -D_PCTERM -I../psplink
LIBS=-lreadline

PREFIX=$(shell psp-config --pspdev-path 2> /dev/null)

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(CXX) -o $@ $^ $(LIBS)

install: $(OUTPUT)
	@echo "Installing $(OUTPUT)..."
	@if ( test $(PREFIX) ); then { mkdir -p $(PREFIX)/bin && cp $(OUTPUT) $(PREFIX)/bin; } else { echo "Error: psp-config not found!"; exit 1; } fi
	@echo "Done!"

clean:
	rm -f $(OUTPUT) *.o
