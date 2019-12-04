OBJS = config.o irc.o messages.o set.o window.o

CFLAGS = -std=c99 -g -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wshadow
LDFLAGS = -ltls -pthread

.SUFFIXES :
.SUFFIXES : .o .c

picobounce : picobounce.c $(OBJS)
	cc $(CFLAGS) -o $@ picobounce.c $(OBJS) $(LDFLAGS)

config.o : config.c config.h
set.o : set.c set.h
irc.o : irc.c irc.h window.h
messages.o : messages.c messages.h irc.h window.h
window.o : window.c window.h

clean :
	rm picobounce *.o
