#ifndef PICOBOUNCE_SET_H
#define PICOBOUNCE_SET_H

#include <stdbool.h>

struct bucket
{
	struct bucket *next;
	char *key;
};

typedef struct bucket **set_ptr;

set_ptr set_alloc(void);

/* removes all items
 *
 * be sure to call before free(s) to free internals
 */
void set_empty(set_ptr s);

bool set_contains(set_ptr s, char *key);

bool set_add(set_ptr s, char *key);

void set_rm(set_ptr s, char *key);

#endif
