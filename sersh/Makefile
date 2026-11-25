WERROR ?= $(if $(filter true 1,$(CI)),1,0)

OUTPUT=sersh
OBJS=sersh.o

CFLAGS=-Wall $(if $(filter 1,$(WERROR)),-Werror) -g
LIBS=

all: sersh

$(OUTPUT): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f $(OUTPUT) *.o
