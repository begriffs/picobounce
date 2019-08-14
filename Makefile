LIBS = libgsasl
OBJS = window.o

CFLAGS = -std=c99 -g -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Wshadow `pkg-config --cflags $(LIBS)`

LDFLAGS = `pkg-config --libs $(LIBS)`

.SUFFIXES :
.SUFFIXES : .o .c

picobounce : picobounce.c $(OBJS)
	cc $(CFLAGS) -o picobounce picobounce.c $(OBJS) $(LDFLAGS) -ltls

window.o : window.c window.h
