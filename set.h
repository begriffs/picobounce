#ifndef PICOBOUNCE_SET_H
#define PICOBOUNCE_SET_H

#include <pthread.h>
#include <stdbool.h>

struct set
{
	void *elts;
	pthread_mutex_t mut;
};

#define EMPTY_SET ((struct set){NULL, PTHREAD_MUTEX_INITIALIZER})

bool set_contains(struct set *s, char *key);
bool set_add(struct set *s, char *key);
void set_rm(struct set *s, char *key);
void set_rm_all(struct set *s);

#endif
