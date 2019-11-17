#ifndef PICOBOUNCE_CLIENT_H
#define PICOBOUNCE_CLIENT_H

#define TCP_BACKLOG  1

#include "config.h"
#include "messages.h"

extern struct msg_log *g_from_client, *g_to_client;

void client_read(struct main_config *cfg);

#endif
