#include <stdlib.h>
#include <string.h>

#include <derp/common.h>

#include "set.h"

static int sentinel;

struct set *set_new(void)
{
	struct set *s = malloc(sizeof *s);
	if (!s)
		return NULL;
	s->elts = tm_new(derp_strcmp, NULL);
	/* TODO: evaluate if we need tm_dtor */
	if (!s->elts)
	{
		free(s);
		return NULL;
	}
	s->mut = PTHREAD_MUTEX_INITIALIZER;
	return s;
}

void set_rm_all(struct set *s)
{
	tm_clear(s->elts);
}

bool set_contains(struct set *s, char *key)
{
	bool ret;
	pthread_mutex_lock(&s->mut);
	ret = tm_at(s->elts, key) != NULL;
	pthread_mutex_unlock(&s->mut);
	return ret;
}

bool set_add(struct set *s, char *key)
{
	bool ret;
	pthread_mutex_lock(&s->mut);
	ret = tm_insert(s->elts, key, &sentinel);
	pthread_mutex_unlock(&s->mut);
	return ret;
}

void set_rm(struct set *s, char *key)
{
	pthread_mutex_lock(&s->mut);
	tm_remove(s->elts, key);
	pthread_mutex_unlock(&s->mut);
}

list *
set_to_list(struct set *s)
{
	list *l = l_new();
	tm_iter *i;
	struct map_pair *p;
	if (!l)
		return NULL;

	pthread_mutex_lock(&s->mut);
	i = tm_iter_begin(s->elts);
	while (p = tm_iter_next(i))
		l_append(l, p->k);
	pthread_mutex_unlock(&s->mut);
	tm_iter_free(i);

	/* caller is responsible for freeing it */
	return l;
}
