#include <stdlib.h>
#include <string.h>

#include "window.h"

window *window_alloc(size_t cap)
{
	window *ret = malloc(sizeof(window));
	if (!ret)
		return NULL;
	ret->reader = ret->buf = calloc(1, 2*cap + 1);
	if (!ret->buf)
	{
		free(ret);
		return NULL;
	}
	ret->exhausted = true;
	return ret;
}

void window_free(window *w)
{
	if (!w)
		return;
	if (w->buf)
		free(w->buf);
	free(w);
}
char *window_next(window *w)
{
	char *old;

	if (!w || w->exhausted)
		return NULL;

	for (old = w->reader; *w->reader; w->reader++)
		if (*w->reader == '\n' || *w->reader == '\r')
		{
			*w->reader++ = '\0';
			if (*w->reader == '\n' || *w->reader == '\r')
				*w->reader++ = '\0';

			return old;
		}

	/* must have hit NUL char */
	w->reader = old;
	w->exhausted = true;
	return NULL;
}

bool window_fill(window *w, char *s)
{
	size_t remainder;

	if (!w || !w->exhausted)
		return false;
	/* shift trailing data to start of buf */
	remainder = strlen(w->reader);
	memmove(w->buf, w->reader, remainder);
	/* append new data */
	strcpy(w->buf+remainder, s);
	/* rewind reader */
	w->reader = w->buf;
	w->exhausted = false;
	return true;
}
