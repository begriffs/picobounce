#include <stdlib.h>
#include <string.h>

#include "sasl.h"
#include "base64.h"

bool extract_user_pass(char *b64, char *user, char *pass)
{
	char throwaway[MAX_SASL_FIELD+1];
	char *fields[3] = { user, throwaway, pass };
	char *decoded, *s;
	int i, decoded_len;

	/* assumption: all ascii */
	decoded = (char *)unbase64(b64, strlen(b64), &decoded_len);

	if (!decoded)
		return false;

	for (s = decoded, i = 0; i < 3; i++)
	{
		strcpy(fields[i], s);
		s += strlen(fields[i])+1;
	}
	free(decoded);
	return true;
}
