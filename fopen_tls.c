/* Uses funopen from BSD (or libbsd), see configure script */

#include "fopen_tls.h"

#include <errno.h>
#include <stdio.h>
#include <limits.h>

#include <tls.h>

static int readfn(void *ctx, char *buf, int len)
{
	ssize_t ret;
	do
	{
		ret = tls_read((struct tls *)ctx, buf, len);
	} while (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT);
	return ret;
}

static int writefn(void *ctx, const char *buf, int len)
{
	ssize_t ret;
	do
	{
		ret = tls_write((struct tls *)ctx, buf, len);
	} while (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT);
	return ret;
}

FILE *fopen_tls(struct tls *ctx)
{
	return funopen(ctx, readfn, writefn, NULL, NULL);
}
