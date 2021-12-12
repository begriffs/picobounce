#include "messages.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct msg *msg_alloc(void)
{
	struct msg *m = malloc(sizeof *m);
	if (!m)
		return NULL;
	*m = (struct msg){0};
	return m;
}

struct msg_log *msg_log_alloc(size_t max_messages)
{
	struct msg_log *log = malloc(sizeof *log);
	list *l = l_new();
	if (!log || !l)
	{
		free(l);
		free(log);
		return NULL;
	}
	log->msgs = l;
	log->max = max_messages;
	pthread_mutex_init(&log->mutex, NULL);
	pthread_cond_init(&log->ready, NULL);
	return log;
}

/* assumes that log->mutex is already unlocked and that log->count > 0 */
static struct msg *_msg_log_consume_unlocked(struct msg_log *log)
{
	assert(l_length(log->msgs) > 0);
	return l_remove_first(log->msgs);
}

void msg_log_add(struct msg_log *log, struct msg *m)
{
	pthread_mutex_lock(&log->mutex);

	l_append(log->msgs, m);
	if (log->max != NO_MESSAGE_LIMIT && l_length(log->msgs) > log->max)
	{
		/* drop oldest message, we're at our limit */
		free(_msg_log_consume_unlocked(log));
	}

	pthread_cond_signal(&log->ready);
	pthread_mutex_unlock(&log->mutex);
}

static void _msg_log_cleanup(void *arg)
{
	struct msg_log *log = arg;
	pthread_mutex_unlock(&log->mutex);
}

struct msg *msg_log_consume(struct msg_log *log)
{
	struct msg *ret;

	pthread_cleanup_push(_msg_log_cleanup, log);
	pthread_mutex_lock(&log->mutex);

	while (l_length(log->msgs) < 1)
		pthread_cond_wait(&log->ready, &log->mutex);
	ret = _msg_log_consume_unlocked(log);
	
	pthread_cleanup_pop(1); /* unlocks the mutex */
	return ret;
}

void msg_log_putback(struct msg_log *log, struct msg *m)
{
	pthread_mutex_lock(&log->mutex);

	l_prepend(log->msgs, m);

	pthread_cond_signal(&log->ready);
	pthread_mutex_unlock(&log->mutex);
}
