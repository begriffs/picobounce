#ifndef PICOBOUNCE_WINDOW_H
#define PICOBOUNCE_WINDOW_H

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
char   *window_next(window *w);
void    window_fill(window *w, char *s);

#endif
