.POSIX:

OBJS = config.o irc.o messages.o set.o window.o

CFLAGS = -std=c99 -pedantic -Wall -Wextra -Wshadow -Wno-missing-braces -D_POSIX_C_SOURCE=200112L -g
LDLIBS = -lpthread

.SUFFIXES :
.SUFFIXES : .o .c

YACC=bison
LEX=flex

include config.mk

irc.a : irc.tab.o irc.lex.o irc.o
	ar r $@ $?

irc.tab.h irc.tab.c : irc.y irc.h
	$(YACC) $(YFLAGS) -d -b irc irc.y

irc.lex.o : irc.tab.h irc.lex.c

irc.lex.c irc.lex.h : irc.l
	$(LEX) $(LFLAGS) --header-file=irc.lex.h --outfile=irc.lex.c irc.l

irc.o : irc.c irc.tab.h irc.lex.h irc.h
	$(CC) $(CFLAGS) -c irc.c

picobounce : picobounce.c $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ picobounce.c $(OBJS) $(LDLIBS)

config.o : config.c config.h messages.h
set.o : set.c set.h
messages.o : messages.c messages.h irc.h window.h
window.o : window.c window.h

clean :
	rm -f picobounce *.o
