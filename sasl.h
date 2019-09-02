#ifndef _SASL_H
#define _SASL_H

#include <stdbool.h>

/* max defined in RFC 4616 */
#define MAX_SASL_FIELD 255

bool extract_creds(const char *b64, char *user, char *pass);
char *encode_creds(const char *user, const char *pass);

#endif
