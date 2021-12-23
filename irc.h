#ifndef PICOBOUNCE_IRC_H
#define PICOBOUNCE_IRC_H

#include <stdio.h>
#include <derp/list.h>
#include <derp/treemap.h>

struct prefix
{
	char *host;
	char *nick;
	char *user;
};

struct irc_message
{
	treemap *tags;
	struct prefix *prefix;
	char *command;
	list *params;
};

struct irc_message *
       irc_message_read(FILE *f);
void   irc_message_print(struct irc_message *m, FILE *f);
void   irc_message_free(struct irc_message *m);
bool   irc_message_is(struct irc_message *m, char *cmd, int n, ...);

#endif
