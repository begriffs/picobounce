#ifndef PICOBOUNCE_UPSTREAM_H
#define PICOBOUNCE_UPSTREAM_H

#include <regex.h>

#include "config.h"
#include "messages.h"

extern struct msg_log *g_from_upstream, *g_to_upstream;

void upstream_read(struct main_config *cfg);

#endif
