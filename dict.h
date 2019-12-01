#ifndef PICOBOUNCE_DICT_H
#define PICOBOUNCE_DICT_H

#include <stdbool.h>

bool dict_contains(char *key);

bool dict_add(char *key);

void dict_rm(char *key);

#endif
