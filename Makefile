OBJS = client.o config.o irc.o messages.o upstream.o window.o

CFLAGS = -std=c99 -g -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wshadow
LDFLAGS = -ltls -pthread

.SUFFIXES :
.SUFFIXES : .o .c

picobounce : picobounce.c $(OBJS)
	cc $(CFLAGS) -o $@ picobounce.c $(OBJS) $(LDFLAGS)

client.o : client.c client.h messages.h upstream.h window.h
config.o : config.c config.h
irc.o : irc.c irc.h
messages.o : messages.c messages.h
upstream.o : upstream.c client.h upstream.h window.h
window.o : window.c window.h

clean :
	rm picobounce *.o
