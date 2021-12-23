#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "irc.h"

/* to turn on parser debug logs if desired */
extern int ircv3_debug;

char *encode_creds(const char *user, const char *pass);

int main(int argc, char **argv)
{
	struct irc_message *m;
	bool first_message = true;
	FILE *fpass;
	size_t max_cred_len = 100;
	char *user, *pass;

	//ircv3_debug = 1;

	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s nick passfile\n", *argv);
		return EXIT_FAILURE;
	}
	user = argv[1];
	if ((fpass = fopen(argv[2], "r")) == NULL)
	{
		fprintf(stderr,
				"Can't open credentials file: \"%s\"\n", argv[1]);
		return EXIT_FAILURE;
	}

	if (getline(&pass, &max_cred_len, fpass) < 1)
	{
		fprintf(stderr, "Failed to read password\n");
		return EXIT_FAILURE;
	}
	pass[strcspn(pass, "\n")] = '\0';

	fclose(fpass);

	setvbuf(stdout, NULL, _IOLBF, 0);

	while ((m = irc_message_read(stdin)) != NULL)
	{
		if (first_message)
		{
			puts("CAP REQ :sasl");
			printf("NICK %s\n", user);
			puts("USER singleuser localhost localhost :singleuser");
			first_message = false;
		}
		else if (irc_message_is(m, "CAP", 3, "*",          "ACK", "sasl") ||
				 irc_message_is(m, "CAP", 3, "singleuser", "ACK", "sasl"))
		{
			puts("AUTHENTICATE PLAIN");
		}
		else if (irc_message_is(m, "AUTHENTICATE", 1, "+"))
		{
			printf("AUTHENTICATE %s\n", encode_creds(user, pass));
		}
		else if (irc_message_is(m, "903", 0))
		{
			puts("CAP END");
			fprintf(stderr, "Authenticated as %s\n", user);
			return EXIT_SUCCESS;
		}
		else if (irc_message_is(m, "904", 0))
		{
			puts("CAP END");
			fprintf(stderr, "Authentication failed for user %s\n", user);
			return EXIT_FAILURE;
		}
		irc_message_free(m);
	}
	return EXIT_SUCCESS;
}

int b64_enc_len(int srclen);
int b64_encode(const char *src, int len, char *dst, int dstlen);

/* caller must free the return string */
char *encode_creds(const char *user, const char *pass)
{
	size_t n, len = 2*(strlen(user)+1)+strlen(pass);
	char *plaintext = malloc(len+1), *ret;

	if (!plaintext)
		return NULL;
	sprintf(plaintext, "%s%c%s%c%s", user, '\0', user, '\0', pass);

	n = b64_enc_len(len) + 1;
	ret = calloc(1, n);

	if (!ret)
		goto done;
	if (b64_encode(plaintext, len, ret, n) < 0)
	{
		free(ret);
		ret = NULL;
	}

done:
	free(plaintext);
	return ret;
}

/*
 ***********************************************
 * The following base64 stuff is from PostgreSQL
 ***********************************************
*/

const char _base64[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * b64_encode
 *
 * Encode into base64 the given string.  Returns the length of the encoded
 * string, and -1 in the event of an error with the result buffer zeroed
 * for safety.
 */
int b64_encode(const char *src, int len, char *dst, int dstlen)
{
	char *p;
	const char *s, *end = src + len;
	int pos = 2;
	unsigned long buf = 0;

	s = src;
	p = dst;

	while (s < end)
	{
		buf |= (unsigned char) *s << (pos << 3);
		pos--;
		s++;

		/* write it out */
		if (pos < 0)
		{
			/*
			 * Leave if there is an overflow in the area allocated for the
			 * encoded string.
			 */
			if ((p - dst + 4) > dstlen)
				goto error;

			*p++ = _base64[(buf >> 18) & 0x3f];
			*p++ = _base64[(buf >> 12) & 0x3f];
			*p++ = _base64[(buf >> 6) & 0x3f];
			*p++ = _base64[buf & 0x3f];

			pos = 2;
			buf = 0;
		}
	}
	if (pos != 2)
	{
		/*
		 * Leave if there is an overflow in the area allocated for the encoded
		 * string.
		 */
		if ((p - dst + 4) > dstlen)
			goto error;

		*p++ = _base64[(buf >> 18) & 0x3f];
		*p++ = _base64[(buf >> 12) & 0x3f];
		*p++ = (pos == 0) ? _base64[(buf >> 6) & 0x3f] : '=';
		*p++ = '=';
	}

	assert((p - dst) <= dstlen);
	return p - dst;

error:
	memset(dst, 0, dstlen);
	return -1;
}

/*
 * b64_enc_len
 *
 * Returns to caller the length of the string if it were encoded with
 * base64 based on the length provided by caller.  This is useful to
 * estimate how large a buffer allocation needs to be done before doing
 * the actual encoding.
 */
int b64_enc_len(int srclen)
{
	/* 3 bytes will be converted to 4 */
	return (srclen + 2) * 4 / 3;
}
