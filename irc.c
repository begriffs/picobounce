#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include "irc.h"
#include "window.h"

#define TCP_BACKLOG 1

static int b64_enc_len(int srclen);
static int b64_dec_len(int srclen);
static int b64_encode(const char *src, int len, char *dst, int dstlen);
static int b64_decode(const char *src, int len, char *dst, int dstlen);

bool extract_creds(const char *b64, char *user, char *pass)
{
	char throwaway[MAX_SASL_FIELD+1];
	char *fields[3] = { throwaway, user, pass };
	char *decoded, *s;
	int i, n;

	n = b64_dec_len(strlen(b64)) + 1;
	decoded = calloc(1, n);

	if (!decoded || b64_decode(b64, strlen(b64), decoded, n) < 0)
	{
		if (decoded)
			free(decoded);
		return false;
	}

	for (s = decoded, i = 0; i < 3 && s < decoded+n; i++)
	{
		strcpy(fields[i], s);
		s += strlen(fields[i])+1;
	}
	free(decoded);
	/* all three components provided? */
	return i==2;
}

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

ssize_t tls_printf(struct tls *tls, const char *fmt, ...)
{
	va_list ap;
	ssize_t ret;
	char out[MAX_IRC_MSG+1];

	va_start(ap, fmt);

	if (vsnprintf(out, MAX_IRC_MSG, fmt, ap) > MAX_IRC_MSG)
		fprintf(stderr, "tls_printf(): message truncated: %s\n", fmt);

	printf("<- %s", out);
	if ((ret = tls_write(tls, out, strlen(out))) < 0)
		fprintf(stderr, "tls_write(): %s\n", tls_error(tls));

	va_end(ap);
	return ret;
}

/* adapted from
 * http://pubs.opengroup.org/onlinepubs/9699919799/functions/getaddrinfo.html
 *
 * svc: either a name like "ftp" or a port number as string
 *
 * Returns: socket file desciptor, or negative error value
 */
int negotiate_listen(const char *svc)
{
	int sock, e, reuseaddr=1;
	struct addrinfo hints = {0}, *addrs, *ap;

	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;
	if ((e = getaddrinfo(NULL, svc, &hints, &addrs)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(e));
		return -1;
	}

	for (ap = addrs; ap != NULL; ap = ap->ai_next)
	{
		sock = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol);
		if (sock < 0)
			continue;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		               &reuseaddr,sizeof(reuseaddr)) < 0)
			perror("setsockopt(REUSEADDR)");
		if (bind(sock, ap->ai_addr, ap->ai_addrlen) == 0)
			break; /* noice */
		perror("Failed to bind");
		close(sock);
	}
	freeaddrinfo(addrs);

	if (ap == NULL)
	{
		fprintf(stderr, "Could not bind\n");
		return -1;
	}
	if (listen(sock, TCP_BACKLOG) < 0)
	{
		perror("listen()");
		close(sock);
		return -1;
	}

	return sock;
}

struct irc_caps
caps_requested(char *req)
{
	char *tok, *state = NULL;
	struct irc_caps caps = {0};
	char s[MAX_IRC_MSG+1] = {0};
	int neg;

	/* strtok_r modifies string, make a copy */
	strncat(s, req, MAX_IRC_MSG);

	for (tok = strtok_r(s, " ", &state);
	     tok;
	     tok = strtok_r(NULL, " ", &state))
	{
		neg = *tok == '-';
		if (neg)
			tok++;
		if (strcmp("sasl", tok) == 0)
			caps.sasl = !neg;
		else if (strstr(tok, "server-time"))
			caps.server_time = !neg;
		else /* unknown */
			caps.error = true;
	}
	return caps;
}

struct irc_caps
client_auth(struct tls *tls, const char *local_user, const char *local_pass)
{
	char msg[MAX_IRC_MSG+1],
	     nick[MAX_IRC_NICK+1] = "*";
	ssize_t amt_read;
	window *w;
	struct irc_caps caps = {0};

	if (!(w = window_alloc(MAX_IRC_MSG)))
	{
		fputs("Failed to allocate irc message buffer\n", stderr);
		caps.error = 1;
		return caps;
	}

	while ((amt_read = tls_read(tls, msg, MAX_IRC_MSG)) > 0)
	{
		char *line;

		msg[amt_read] = '\0';
		window_fill(w, msg);
		while ((line = window_next(w)) != NULL)
		{
			printf("-> %s\n", line);
			if (strncmp(line, "NICK ", 5) == 0)
			{
				snprintf(nick, MAX_IRC_NICK, "%s", line+5);
				printf("!! Nick is now %s\n", nick);
			}
			else if (strncmp(line, "CAP REQ ", 8) == 0)
			{
				char *capreq = line+8;
				caps = caps_requested(capreq);
				tls_printf(tls, ":localhost CAP %s %s :%s\n",
						nick, caps.error ? "NAK" : "ACK", capreq);
			}
			else if (strncmp(line, "CAP LS 302", 8) == 0)
				tls_printf(tls, ":localhost CAP %s LS :sasl server-time\n", nick);
			else if (strncmp(line, "AUTHENTICATE ", 13) == 0)
			{
				char *auth = line+13;
				if (strcmp(auth, "PLAIN") == 0)
					tls_printf(tls, "AUTHENTICATE +\n");
				else if (strcmp(auth, "*") == 0)
				{
					tls_printf(tls,
							":localhost 906 %s :SASL authentication aborted\n", nick);
					/* keep trying I guess */
				}
				else
				{
					char username[MAX_SASL_FIELD],
					     password[MAX_SASL_FIELD];
					extract_creds(auth, username, password);
					if (strcmp(local_user, username) == 0 ||
							strcmp(local_pass, password) == 0)
					{
						tls_printf(tls,
								":localhost 903 %s :SASL authentication successful\n", nick);
						window_free(w);
						return caps;
					}
					else
					{
						caps.error = 1;
						tls_printf(tls,
								":localhost 904 %s :SASL authentication failed\n", nick);
						return caps;
					}
				}
			}
		}
	}

	if (amt_read < 0)
		fprintf(stderr, "tls_read(): %s\n", tls_error(tls));

	window_free(w);
	return caps;
}

/*
 ***********************************************
 * The following base64 stuff is from PostgreSQL
 ***********************************************
*/

static const char _base64[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const signed char b64lookup[128] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
};

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
 * b64_decode
 *
 * Decode the given base64 string.  Returns the length of the decoded
 * string on success, and -1 in the event of an error with the result
 * buffer zeroed for safety.
 */
int b64_decode(const char *src, int len, char *dst, int dstlen)
{
	const char *srcend = src + len, *s = src;
	char *p = dst;
	char c;
	int b = 0;
	unsigned long buf = 0;
	int pos = 0, end = 0;

	while (s < srcend)
	{
		c = *s++;

		/* Leave if a whitespace is found */
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			goto error;

		if (c == '=')
		{
			/* end sequence */
			if (!end)
			{
				if (pos == 2)
					end = 1;
				else if (pos == 3)
					end = 2;
				else
				{
					/*
					 * Unexpected "=" character found while decoding base64
					 * sequence.
					 */
					goto error;
				}
			}
			b = 0;
		}
		else
		{
			b = -1;
			if (c > 0 && c < 127)
				b = b64lookup[(unsigned char) c];
			if (b < 0)
			{
				/* invalid symbol found */
				goto error;
			}
		}
		/* add it to buffer */
		buf = (buf << 6) + b;
		pos++;
		if (pos == 4)
		{
			/*
			 * Leave if there is an overflow in the area allocated for the
			 * decoded string.
			 */
			if ((p - dst + 1) > dstlen)
				goto error;
			*p++ = (buf >> 16) & 255;

			if (end == 0 || end > 1)
			{
				/* overflow check */
				if ((p - dst + 1) > dstlen)
					goto error;
				*p++ = (buf >> 8) & 255;
			}
			if (end == 0 || end > 2)
			{
				/* overflow check */
				if ((p - dst + 1) > dstlen)
					goto error;
				*p++ = buf & 255;
			}
			buf = 0;
			pos = 0;
		}
	}

	if (pos != 0)
	{
		/*
		 * base64 end sequence is invalid.  Input data is missing padding, is
		 * truncated or is otherwise corrupted.
		 */
		goto error;
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

/*
 * b64_dec_len
 *
 * Returns to caller the length of the string if it were to be decoded
 * with base64, based on the length given by caller.  This is useful to
 * estimate how large a buffer allocation needs to be done before doing
 * the actual decoding.
 */
int b64_dec_len(int srclen)
{
	return (srclen * 3) >> 2;
}
