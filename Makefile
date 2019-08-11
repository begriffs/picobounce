CFLAGS = -std=c99 -DNDEBUG -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Wshadow `pkg-config --cflags libtls`

LDFLAGS = `pkg-config --libs libtls`

.SUFFIXES :
.SUFFIXES : .o .c

all : picobounce

picobounce : picobounce.o window.o

window.o : window.c window.h
