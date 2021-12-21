.POSIX:

# using POSIX 7th ed (2008) for getline(3),
# otherwise 6th ed would suffice
CFLAGS = -std=c99 -pedantic -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wshadow -g
LDLIBS = -lpthread

.SUFFIXES :
.SUFFIXES : .o .c

YACC=bison
LEX=flex

include config.mk

### programs ###

all : irc_auth irc_relay tailbuf

irc_auth : irc_auth.c irc.h irc.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ irc_auth.c irc.a $(LDLIBS)

irc_relay : irc_relay.c irc.h irc.a set.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ irc_relay.c set.o irc.a $(LDLIBS)

tailbuf : tailbuf.c

### message parsing ###

irc.a : irc.tab.o irc.lex.o irc.o
	ar r $@ $?

irc.tab.h irc.tab.c : irc.y irc.h
	$(YACC) $(YFLAGS) -d -b irc irc.y

irc.lex.o : irc.tab.h irc.lex.c

irc.lex.c irc.lex.h : irc.l
	$(LEX) $(LFLAGS) --header-file=irc.lex.h --outfile=irc.lex.c irc.l

### helpers ###

irc.o : irc.c irc.tab.h irc.lex.h irc.h
	$(CC) $(CFLAGS) -c irc.c

set.o : set.c set.h

clean :
	rm -f irc_auth irc_relay *.o *.a *.lex.[ch] *.tab.[ch]
