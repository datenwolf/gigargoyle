CFLAGS = -O2 -Wall -g
OBJS_gg = gigargoyle.o command_line_arguments.o packets.o fifo.o

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
