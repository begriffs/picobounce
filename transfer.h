#ifndef PICOBOUNCE_UPSTREAM_H
#define PICOBOUNCE_UPSTREAM_H

#include "config.h"
#include "messages.h"

extern struct msg_log *g_from_upstream, *g_from_client;

void client_read(struct main_config *cfg);
void upstream_read(struct main_config *cfg);

#endif
