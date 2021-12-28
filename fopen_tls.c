/* Uses funopen from BSD (or libbsd), see configure script */

#include "fopen_tls.h"

#include <errno.h>
#include <stdio.h>
#include <limits.h>

#include <tls.h>

static int readfn(void *ctx, char *buf, int len)
{
	ssize_t ret;
	if (sizeof len > sizeof ret && len > SSIZE_MAX)
	{
		errno = EINVAL;
		return -1;
	}
	do
	{
		ret = tls_read(ctx, buf, len);
	} while (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT);
	return (int)ret;
}

static int writefn(void *ctx, const char *buf, int len)
{
	ssize_t ret;
	if (sizeof len > sizeof ret && len > SSIZE_MAX)
	{
		errno = EINVAL;
		return -1;
	}
	int togo = len;
	while (togo > 0)
	{
		ret = tls_write(ctx, buf, togo);
		if (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT)
			continue;
		if (ret == -1)
			return -1;
		buf  += ret;
		togo -= ret;
	}
	return len;
}

/* On systems with funopen, fpos_t is really off_t. We
 * pick the latter to emphasize it's a signed integral type
 * (and -1 indicates an error) */
static off_t seekfn(void *ctx, off_t offset, int whence)
{
	(void)ctx; (void)offset; (void)whence;
	errno = ESPIPE;
	return -1;
}

static int closefn(void *ctx)
{
	return tls_close((struct tls *)ctx);
}

FILE *fopen_tls(struct tls *ctx)
{
	return funopen(ctx, readfn, writefn, seekfn, closefn);
}
