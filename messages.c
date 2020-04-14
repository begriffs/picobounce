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
	if (!log)
		return NULL;
	*log = (struct msg_log){0};
	log->max = max_messages;
	pthread_mutex_init(&log->mutex, NULL);
	pthread_cond_init(&log->ready, NULL);
	return log;
}

/* assumes that log->mutex is already unlocked and that log->count > 0 */
static struct msg *_msg_log_consume_unlocked(struct msg_log *log)
{
	struct msg *ret;
	assert(log->count > 0);

	ret = log->front;
	log->front = log->front->next;
	if (log->front == NULL)
		log->rear = NULL;
	else
		log->front->prev = NULL;
	log->count--;
	return ret;
}

void msg_log_add(struct msg_log *log, struct msg *m)
{
	pthread_mutex_lock(&log->mutex);

	if (log->rear == NULL)
		log->front = log->rear = m;
	else
	{
		m->prev = log->rear;
		log->rear->next = m;
		log->rear = m;
	}
	log->count++;
	if (log->max != NO_MESSAGE_LIMIT && log->count > log->max)
	{
		/* drop oldest message, we're at our limit */
		free(_msg_log_consume_unlocked(log));
	}

	pthread_cond_signal(&log->ready);
	pthread_mutex_unlock(&log->mutex);
}

struct msg *msg_log_consume(struct msg_log *log)
{
	struct msg *ret;
	pthread_mutex_lock(&log->mutex);

	while (log->count < 1)
		pthread_cond_wait(&log->ready, &log->mutex);
	ret = _msg_log_consume_unlocked(log);

	pthread_mutex_unlock(&log->mutex);
	return ret;
}

void msg_log_putback(struct msg_log *log, struct msg *m)
{
	pthread_mutex_lock(&log->mutex);

	if (log->front == NULL)
		log->rear = log->front = m;
	else
	{
		m->next = log->front;
		log->front->prev = m;
		log->front = m;
	}
	log->count++;

	pthread_cond_signal(&log->ready);
	pthread_mutex_unlock(&log->mutex);
}
