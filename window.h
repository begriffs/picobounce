#ifndef _WINDOW_H
#define _WINDOW_H

#include <stdbool.h>
#include <stddef.h>

typedef struct window
{
	char *buf;

	char *reader;
	bool exhausted;
} window;

window *window_alloc(size_t cap);
void    window_free(window *w);
char   *window_tok(window *w, char tok);
bool    window_fill(window *w, char *s);

#endif
