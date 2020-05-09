#ifndef PICOBOUNCE_TLS_DEBUG_H
#define PICOBOUNCE_TLS_DEBUG_H

#include <stdbool.h>
#include <tls.h>

bool tls_dump_keylog(struct tls *tls);

#endif
