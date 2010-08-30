#CC=diet gcc -Os
CFLAGS=-O2 -Wall -g

CFLAGS+=-DHAS_ARGP_ERROR  # <- enable these for linux/glibc
CFLAGS+=-DHAS_ARGP_USAGE  # <- enable these for linux/glibc
CFLAGS+=-DHAS_ARGP_PARSE  # <- enable these for linux/glibc

OBJS=gigargoyle.o command_line_arguments.o fifo.o packets.o
OBJS_gg = gigargoyle.o command_line_arguments.o fifo.o packets.o

OBJS_acabspectrum = acabspectrum.o gg_simple_client.o
LIBS_acabspectrum = -lm -lfftw3 -ljack

OBJS_simpleclient = simpleclient.o gg_simple_client.o
LIBS_simpleclient = -lm

OBJS=$(OBJS_gg) $(OBJS_acabspectrum) $(OBJS_simpleclient)

all: gigargoyle testpacket acabspectrum simpleclient

gigargoyle: $(OBJS_gg)

acabspectrum: $(OBJS_acabspectrum)
	$(CC) $(LIBS_acabspectrum) -o acabspectrum $(OBJS_acabspectrum)

simpleclient: $(OBJS_simpleclient)
	$(CC) $(LIBS_simpleclient) -o simpleclient $(OBJS_simpleclient)

clean:
	rm -f gigargoyle $(OBJS)
