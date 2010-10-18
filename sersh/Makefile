OUTPUT=sersh
OBJS=sersh.o

CFLAGS=-Wall -g 
LIBS=

all: sersh

$(OUTPUT): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f $(OUTPUT) *.o
