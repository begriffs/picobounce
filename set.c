#include <stdlib.h>
#include <string.h>

#include "set.h"

#define HASHSZ 101

set *set_alloc(void)
{
	set *s = malloc(sizeof *s);
	if (!s)
		return NULL;
	s->sz = HASHSZ;
	s->hashtab = malloc(s->sz * (sizeof *s->hashtab));
	if (!s->hashtab)
	{
		free(s);
		return NULL;
	}
	set_empty(s);
	return s;
}

void set_empty(set *s)
{
	size_t i;
	struct bucket *cur, *next;

	for (i = 0; i < s->sz; i++)
	{
		cur = s->hashtab[i];
		while (cur)
		{
			next = cur->next;
			free(cur->key);
			free(cur);
			cur = next;
		}
		s->hashtab[i] = NULL;
	}
}

static unsigned long
djb2hash(const unsigned char *str)
{
	unsigned long hash = 5381;
	int c;

	if (str)
		while ( (c = *str++) )
			hash = hash * 33 + c;
	return hash;
}

static struct bucket *set_lookup(set *s, char *key)
{
	struct bucket* np;
	for (np = s->hashtab[djb2hash(key) % s->sz]; np; np = np->next)
		if (strcmp(key, np->key) == 0)
			return np;
	return NULL;
}

bool set_contains(set *s, char *key)
{
	return set_lookup(s, key) != NULL;
}

bool set_add(set *s, char *key)
{
	struct bucket *np;
	unsigned long h;

	if ((np = set_lookup(s, key)) == NULL)
	{
		np = malloc(sizeof(*np));
		if (np == NULL || (np->key = strdup(key)) == NULL)
			return false;
		h = djb2hash(key) % s->sz;
		np->next = s->hashtab[h];
		s->hashtab[h] = np;
	}
	return true;
}

void set_rm(set *s, char *key)
{
	struct bucket *np, *prev;
	unsigned long h = djb2hash(key) % s->sz;
	if ((np = s->hashtab[h]) == NULL)
		return;
	for (prev = NULL; np; np = np->next, prev = np)
	{
		if (strcmp(key, np->key) == 0)
		{
			if (prev == NULL)
				s->hashtab[h] = np->next;
			else
				prev->next = np->next;
			free(np->key);
			free(np);
			break;
		}
	}
}
