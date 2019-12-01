#include <stdlib.h>
#include <string.h>

#include "set.h"

struct bucket
{
	struct bucket *next;
	char *key;
};

#define HASHSZ 101

static struct bucket *hashtab[HASHSZ];

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

static struct bucket *set_lookup(char *key)
{
	struct bucket* np;
	for (np = hashtab[djb2hash(key)]; np; np = np->next)
		if (strcmp(key, np->key) == 0)
			return np;
	return NULL;
}

bool set_contains(char *key)
{
	return set_lookup(key) != NULL;
}

bool set_add(char *key)
{
	struct bucket *np;
	unsigned long h;

	if ((np = set_lookup(key)) == NULL)
	{
		np = malloc(sizeof(*np));
		if (np == NULL || (np->key = strdup(key)) == NULL)
			return false;
		h = djb2hash(key);
		np->next = hashtab[h];
		hashtab[h] = np;
	}
	return true;
}

void set_rm(char *key)
{
	struct bucket *np, *prev;
	unsigned long h = djb2hash(key);
	if ((np = hashtab[h]) == NULL)
		return;
	for (prev = NULL; np; np = np->next, prev = np)
	{
		if (strcmp(key, np->key) == 0)
		{
			if (prev == NULL)
				hashtab[h] = np->next;
			else
				prev->next = np->next;
			free(np->key);
			free(np);
			break;
		}
	}
}
