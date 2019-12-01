#ifndef PICOBOUNCE_SET_H
#define PICOBOUNCE_SET_H

#include <stdbool.h>

bool set_contains(char *key);

bool set_add(char *key);

void set_rm(char *key);

#endif
