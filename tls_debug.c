#include "tls_debug.h"

#include <openssl/ssl.h>
#include <stdlib.h>

/* TOTAL HACK: a copy of the tls structure from libtls/tls_internal.h
 *
 * When libtls changes it could break ABI compatibility
 */
struct tls_internal {
	struct tls_config *config;
	struct tls_keypair *keypair;

	struct {
		char *msg;
		int num;
		int tls;
	} error;

	uint32_t flags;
	uint32_t state;

	char *servername;
	int socket;

	SSL *ssl_conn;
	SSL_CTX *ssl_ctx;

	struct tls_sni_ctx *sni_ctx;

	X509 *ssl_peer_cert;
	STACK_OF(X509) *ssl_peer_chain;

	struct tls_conninfo *conninfo;

	struct tls_ocsp *ocsp;

	tls_read_cb read_cb;
	tls_write_cb write_cb;
	void *cb_arg;
};

static void _writehex(FILE *fp, const unsigned char* s, size_t len)
{
	while (len-- > 0)
		fprintf(fp, "%2hhx", *s++);
}

bool tls_dump_keylog(struct tls *tls)
{
	unsigned int len_key, len_id;
	SSL_SESSION *sess;
	unsigned char key[256];
	const unsigned char *id;
	FILE *fp;
	const char *path = getenv("SSLKEYLOGFILE");
	if (!path)
		return false;

	/* hacky type aliasing */
	sess = SSL_get_session(((struct tls_internal*)tls)->ssl_conn);
	if (!sess)
	{
		fprintf(stderr, "Failed to get SSL session for TLS\n");
		return false;
	}
	len_key = SSL_SESSION_get_master_key(sess, key, sizeof key);
	id      = SSL_SESSION_get_id(sess, &len_id);

	if ((fp = fopen(path, "w")) == NULL)
	{
		fprintf(stderr, "Unable to write keylog to '%s'\n", path);
		return false;
	}
	fputs("RSA Session-ID:", fp);
	_writehex(fp, id, len_id);
	fputs(" Master-Key:", fp);
	_writehex(fp, key, len_key);
	fputs("\n", fp);
	fclose(fp);
	return true;
}
