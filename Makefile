OBJS = window.o sasl.o

CFLAGS = -std=c99 -g -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Wshadow

.SUFFIXES :
.SUFFIXES : .o .c

picobounce : picobounce.c $(OBJS)
	cc $(CFLAGS) -o picobounce picobounce.c $(OBJS) -ltls

window.o : window.c window.h

sasl.o : sasl.c sasl.h

clean :
	rm picobounce *.o
