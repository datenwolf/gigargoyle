CFLAGS=-O2 -Wall -g
OBJS=gigargoyle.o command_line_arguments.o fifo.o packets.o

OBJS=gigargoyle.o command_line_arguments.o packets.o fifo.o

all:gigargoyle testpacket

gigargoyle: $(OBJS)

clean:
	rm -f gigargoyle $(OBJS)
