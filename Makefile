CFLAGS=-O2 -Wall

OBJS=gigargoyle.o command_line_arguments.o packets.o

all: gigargoyle

gigargoyle: ${OBJS}
	${CC} ${CFLAGS} -o gigargoyle ${OBJS}

clean:
	rm -f gigargoyle ${OBJS}
