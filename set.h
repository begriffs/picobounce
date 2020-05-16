#ifndef PICOBOUNCE_SET_H
#define PICOBOUNCE_SET_H

#include <stdbool.h>

#define EMPTY_SET NULL

void set_free(void *s);
bool set_contains(void *s, char *key);
bool set_add(void *s, char *key);
void set_rm(void *s, char *key);

#endif
