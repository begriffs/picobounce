#ifndef PICOBOUNCE_MESSAGES_H
#define PICOBOUNCE_MESSAGES_H

#include <pthread.h>

#include "irc.h"

struct msg
{
	time_t at;
	char   text[MAX_IRC_MSG];
	struct msg *older, *newer;
};

struct msg *msg_alloc(void);

struct msg_log
{
	struct  msg *oldest, *newest;
	ssize_t count;

	pthread_mutex_t mutex;
	pthread_cond_t  ready;
};

struct msg_log *msg_log_alloc(void);

void msg_log_add(struct msg_log *log, struct msg *m);
struct msg *msg_log_consume(struct msg_log *log);
void msg_log_putback(struct msg_log *log, struct msg *m);

#endif
