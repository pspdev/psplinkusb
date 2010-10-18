OUTPUT=ttylink
OBJS=ttylink.o

CFLAGS=-Wall -g 
LIBS=-lreadline -lcurses

all: ttylink

$(OUTPUT): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f $(OUTPUT) *.o
