OUTPUT=sersh
OBJS=sersh.o

CFLAGS=-Wall -Werror -g
LIBS=

all: sersh

$(OUTPUT): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f $(OUTPUT) *.o
