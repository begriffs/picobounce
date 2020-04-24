.POSIX:

OBJS = config.o irc.o messages.o set.o window.o

CFLAGS = -std=c99 -pedantic -Wall -Wextra -Wshadow -Wno-missing-braces -D_POSIX_C_SOURCE=200112L -g
LDLIBS = -lpthread

.SUFFIXES :
.SUFFIXES : .o .c

include config.mk

picobounce : picobounce.c $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ picobounce.c $(OBJS) $(LDLIBS)

config.o : config.c config.h messages.h
set.o : set.c set.h
irc.o : irc.c irc.h window.h
messages.o : messages.c messages.h irc.h window.h vendor/queue.h
window.o : window.c window.h

clean :
	rm -f picobounce *.o
