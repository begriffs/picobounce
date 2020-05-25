#ifndef PICOBOUNCE_SET_H
#define PICOBOUNCE_SET_H

#include <pthread.h>
#include <stdbool.h>

#include "vendor/queue.h"

struct set
{
	void *elts;
	pthread_mutex_t mut;
};

#define EMPTY_SET ((struct set){NULL, PTHREAD_MUTEX_INITIALIZER})

struct set_list_item
{
	char *key;
	SLIST_ENTRY(set_list_item) link;
};
SLIST_HEAD(set_list, set_list_item);

bool set_contains(struct set *s, char *key);
bool set_add(struct set *s, char *key);
void set_rm(struct set *s, char *key);
void set_rm_all(struct set *s);
struct set_list *set_to_list(struct set *s);

#endif
