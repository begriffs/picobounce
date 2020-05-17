#include <stdlib.h>
#include <string.h>

#include <search.h>

#include "set.h"

static int
_always_equal(const void *a, const void *b)
{
	(void) a;
	(void) b;
    return 0;
}

void set_rm_all(struct set *s)
{
	void *t;
	while ((t = s->elts) != NULL)
	{
		tdelete(t, &s->elts, _always_equal);
		free(t);
	}
}

/* TODO: is casting a safe practice? */
typedef int (*cmpfn)(const void *, const void *);

bool set_contains(struct set *s, char *key)
{
	bool ret;
	pthread_mutex_lock(&s->mut);
	ret = tfind(key, s->elts, (cmpfn)strcmp) != NULL;
	pthread_mutex_unlock(&s->mut);
	return ret;
}

bool set_add(struct set *s, char *key)
{
	bool ret;
	pthread_mutex_lock(&s->mut);
	ret = tsearch(key, s->elts, (cmpfn)strcmp);
	pthread_mutex_unlock(&s->mut);
	return ret;
}

void set_rm(struct set *s, char *key)
{
	void *elt;
	pthread_mutex_lock(&s->mut);
	elt = tfind(key, s->elts, (cmpfn)strcmp);
	if (elt)
	{
		tdelete(key, elt, (cmpfn)strcmp);
		free(elt);
	}
	pthread_mutex_unlock(&s->mut);
}
