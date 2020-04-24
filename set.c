#include <stdlib.h>
#include <string.h>

#include <search.h>

#include "set.h"

typedef int (*cmpfn)(const void *, const void *);

static int
_always_equal(const void *a, const void *b)
{
	(void) a;
	(void) b;
    return 0;
}

void set_free(void *s)
{
	void *elt;
	while ((elt = s) != NULL)
	{
		tdelete(elt, &s, _always_equal);
		free(elt);
	}
}

bool set_contains(void *s, char *key)
{
	return tfind(key, s, (cmpfn)strcmp) != NULL;
}

bool set_add(void *s, char *key)
{
	return tsearch(key, s, (cmpfn)strcmp);
}

void set_rm(void *s, char *key)
{
	tdelete(key, s, (cmpfn)strcmp);
}
