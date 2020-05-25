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

/* thread-specific data
 *
 * the twalk() callback needs to build its list in a global
 * variable, so we'll keep it safe with a pthread key */
static pthread_once_t tsd_key_once = PTHREAD_ONCE_INIT;
static pthread_key_t tsd_key;

static void initialize_key(void)
{
	pthread_key_create(&tsd_key, NULL);
}
/* end thread-specific data */

static void
add_node_to_list(const void *ptr, VISIT order, int level)
{
	(void)level;
	
	if (order == postorder || order == leaf)
	{
		struct set_list *head = pthread_getspecific(tsd_key);
		struct set_list_item *item = malloc(sizeof(*item));

		item->key = strdup(ptr);
		SLIST_INSERT_HEAD(head, item, link);
	}
}

struct set_list *
set_to_list(struct set *s)
{
	struct set_list *head;

	pthread_once(&tsd_key_once, initialize_key);
	if ((head = malloc(sizeof(*head))) == NULL)
		return NULL;
	SLIST_INIT(head);

	pthread_setspecific(tsd_key, head);
	
	pthread_mutex_lock(&s->mut);
	twalk(s->elts, add_node_to_list);
	pthread_mutex_unlock(&s->mut);

	/* caller is responsible for freeing it */
	return head;
}
