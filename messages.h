#ifndef PICOBOUNCE_MESSAGES_H
#define PICOBOUNCE_MESSAGES_H

#include <pthread.h>

#include "irc.h"
#include "derp/list.h"

struct msg
{
	time_t at;
	char   text[MAX_IRC_MSG+1];
};

struct msg_log
{
	list *msgs;
	size_t max;

	pthread_mutex_t mutex;
	pthread_cond_t  ready;
};

#define NO_MESSAGE_LIMIT 0

struct msg *msg_alloc(void);
struct msg_log *msg_log_alloc(size_t max_messages);

void msg_log_add(struct msg_log *log, struct msg *m);
struct msg *msg_log_consume(struct msg_log *log);
void msg_log_putback(struct msg_log *log, struct msg *m);

#endif
