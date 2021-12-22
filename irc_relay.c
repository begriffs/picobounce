#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <time.h>

#include "irc.h"
#include "set.h"

FILE *from_cli, *to_cli,
	 *from_srv, *to_srv;
struct set *active_chans;
char *nick;

void srv_read(void *unused);

char *iso8601time(void);

int main(int argc, char **argv)
{
	pthread_t srv_read_thread;

	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s fifo-from-client fifo-to-client\n", *argv);
		return EXIT_FAILURE;
	}
	if (!(from_cli = fopen(argv[1], "r")) ||
		!(to_cli   = fopen(argv[2], "w")) )
	{
		fprintf(stderr, "Failed to open one of \"%s\" or \"%s\"\n",
		        argv[1], argv[2]);
		return EXIT_FAILURE;
	}
	from_srv = stdin;
	to_srv = stdout;

	/* ensure no delay after messages */
	setvbuf(to_srv, NULL, _IOLBF, 0);
	setvbuf(to_cli, NULL, _IOLBF, 0);

	active_chans = set_new();
	if (!active_chans)
	{
		fputs("Unable to allocate channel list\n", stderr);
		return EXIT_FAILURE;
	}

	pthread_create(&srv_read_thread, NULL,
			(void *(*)(void*))srv_read, NULL);

	struct irc_message *m;
	while ((m = irc_message_read(from_cli)) != NULL)
	{
		if (irc_message_is(m, "PING", 0))
		{
			/* bounce back to client as PONG, same params */
			strcpy(m->command, "PONG");
			irc_message_print(m, to_cli);
		}
		else if (irc_message_is(m, "QUIT", 0))
			; /* client may quit, but we don't */
		else if (irc_message_is(m, "NICK", 0) || irc_message_is(m, "USER", 0))
			; /* ahem, we'll be the judge of that */
		else if (irc_message_is(m, "PRIVMSG", 1, "NickServ"))
			; /* nope, we use SASL */
		else if (irc_message_is(m, "JOIN", 0))
		{
			list_item *arg;
			for (arg = l_first(m->params); arg; arg = arg->next)
			{
				char *chan = arg->data;
				if (set_contains(active_chans, chan))
				{
					/* already joined, refresh for client */
					fprintf(to_srv, "TOPIC %s\n", chan);
					fprintf(to_srv, "NAMES %s\n", chan);
				}
				else
				{
					set_add(active_chans, strdup(chan));
					fprintf(to_srv, "JOIN %s\n", chan);
				}
			}
		}
		else if (irc_message_is(m, "PART", 0))
		{
			list_item *arg;
			for (arg = l_first(m->params); arg; arg = arg->next)
				set_rm(active_chans, (char*)arg->data);
			irc_message_print(m, to_srv);
		}
		else
			irc_message_print(m, to_srv);

		irc_message_free(m);
	}

	/* should not get here */
	return EXIT_SUCCESS;
}

void srv_read(void *unused)
{
	(void)unused;
	struct irc_message *m;

	while ((m = irc_message_read(from_srv)) != NULL)
	{
		if (irc_message_is(m, "PING", 0))
			fputs("PONG\n", to_srv);
		else
		{
			if (!m->tags)
			{
				m->tags = tm_new(derp_strcmp, NULL);
				tm_dtor(m->tags, derp_free, derp_free, NULL);
			}
			tm_insert(m->tags, strdup("time"), iso8601time());
			irc_message_print(m, to_cli);
		}

		/* furthermore */
		if (irc_message_is(m, "KICK", 1, nick) && l_length(m->params) > 1)
			set_rm(active_chans, l_first(m->params)->next->next->data);

		irc_message_free(m);
	}
	set_rm_all(active_chans);
}

char *iso8601time(void)
{
	char iso[64], stamp[64];
	struct timespec tv;
	struct tm tm;
	if(clock_gettime(CLOCK_REALTIME, &tv))
	{
		perror("clock_gettime in iso8601time\n");
		return NULL;
	}
	gmtime_r(&tv.tv_sec, &tm);
	strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%S", &tm);
	snprintf(iso, sizeof iso, "%s.%03LfZ", stamp,
	         ((long double)tv.tv_nsec) / 1e6);
	return strdup(iso);
}
