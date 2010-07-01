CFLAGS=-O2 -Wall

OBJS=gigargoyle.o command_line_arguments.o

all: gigargoyle command_line_arguments.o

gigargoyle: ${OBJS}
	${CC} ${CFLAGS} -o gigargoyle ${OBJS}

clean:
	rm -f gigargoyle ${OBJS}
