WERROR ?= $(if $(filter true 1,$(CI)),1,0)

OUTPUT=ttylink
OBJS=ttylink.o

CFLAGS=-Wall $(if $(filter 1,$(WERROR)),-Werror) -g
LIBS=-lreadline

all: ttylink

$(OUTPUT): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f $(OUTPUT) *.o
