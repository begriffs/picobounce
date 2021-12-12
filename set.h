#ifndef PICOBOUNCE_SET_H
#define PICOBOUNCE_SET_H

#include <pthread.h>
#include <stdbool.h>

#include "derp/treemap.h"
#include "derp/list.h"

struct set
{
	treemap *elts;
	pthread_mutex_t mut;
};

struct set *set_new(void);
bool set_contains(struct set *s, char *key);
bool set_add(struct set *s, char *key);
void set_rm(struct set *s, char *key);
void set_rm_all(struct set *s);
list *set_to_list(struct set *s);

#endif
