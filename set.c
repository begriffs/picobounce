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

static unsigned hash(char *s)
{
	unsigned hashval;

	for (hashval = 0; *s != '\0'; s++)
		hashval = *s + 31 * hashval;
	return hashval % HASHSZ;
}

static struct bucket *set_lookup(char *key)
{
	struct bucket* np;
	for (np = hashtab[hash(key)]; np; np = np->next)
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
	unsigned h;

	if ((np = set_lookup(key)) == NULL)
	{
		np = malloc(sizeof(*np));
		if (np == NULL || (np->key = strdup(key)) == NULL)
			return false;
		h = hash(key);
		np->next = hashtab[h];
		hashtab[h] = np;
	}
	return true;
}

void set_rm(char *key)
{
	struct bucket *np, *prev;
	unsigned h = hash(key);
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
