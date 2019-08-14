LIBS = libtls libgsasl

CFLAGS = -std=c99 -g -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Wshadow `pkg-config --cflags $(LIBS)`

LDFLAGS = `pkg-config --libs $(LIBS)`

.SUFFIXES :
.SUFFIXES : .o .c

all : picobounce

picobounce : picobounce.o window.o

window.o : window.c window.h
