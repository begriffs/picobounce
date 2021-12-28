.POSIX:

OBJS = config.o irc.o messages.o set.o fopen_tls.o

CFLAGS = -std=c99 -pedantic -g -Wall -Wextra -Wshadow
LDLIBS = -lpthread

POSIX = -D_POSIX_C_SOURCE=200809L

.SUFFIXES :
.SUFFIXES : .o .c

.c.o :
	$(CC) $(CFLAGS) $(POSIX) -c $<

YACC=bison
LEX=flex

include config.mk

####################################
### program and objects

picobounce : picobounce.c $(OBJS)
	$(CC) $(CFLAGS) $(POSIX) $(LDFLAGS) -o $@ picobounce.c $(OBJS) $(LDLIBS)

config.o : config.c config.h messages.h
set.o : set.c set.h
messages.o : messages.c messages.h irc.h

# not posix, requires BSD specifics
fopen_tls.o : fopen_tls.c fopen_tls.h
	$(CC) $(CFLAGS) $(CFLAGS_LIBBSD) -c fopen_tls.c

####################################
### bison/flex irc parsing subsystem

irc.a : irc.tab.o irc.lex.o irc.o
	ar r $@ $?

irc.tab.h irc.tab.c : irc.y irc.h
	$(YACC) $(YFLAGS) -d -b irc irc.y

irc.lex.o : irc.tab.h irc.lex.c

irc.lex.c irc.lex.h : irc.l
	$(LEX) $(LFLAGS) --header-file=irc.lex.h --outfile=irc.lex.c irc.l

irc.o : irc.c irc.tab.h irc.lex.h irc.h
	$(CC) $(CFLAGS) $(POSIX) -c irc.c

####################################
### .PHONY

clean :
	rm -f picobounce *.[oa] *.{tab,lex}.[ch]
