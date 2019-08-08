#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <tls.h>

#define TCP_BACKLOG  SOMAXCONN
#define MAX_IRC_MSG  513

/* adapted from
 * http://pubs.opengroup.org/onlinepubs/9699919799/functions/getaddrinfo.html
 *
 * svc: either a name like "ftp" or a port number as string
 *
 * Returns: socket file desciptor, or negative error value
 */
static int
negotiate_listen(const char *svc)
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

/* see
 * https://github.com/bob-beck/libtls/blob/master/TUTORIAL.md#basic-libtls-use
 * and
 * https://github.com/OSUSecLab/Stacco/blob/b4a598c3061537f630526a07545e276cccf298cb/test/libressl/server.c
 */
int main(int argc, char **argv)
{
	int sock;
	struct tls_config *cfg;
	struct tls *tls;
   
	if ((sock = negotiate_listen("6697")) < 0)
	{
		fputs("Unable to listen on 6697\n", stderr);
		return EXIT_FAILURE;
	}

	if ((cfg = tls_config_new()) == NULL)
	{
		fputs("tls_config_new() failed\n", stderr);
		return EXIT_FAILURE;
	}

	/*
	printf("CA Cert file: %s\n", tls_default_ca_cert_file());
	if (tls_config_set_ca_file(cfg, tls_default_ca_cert_file()) < 0)
	{
		fprintf(stderr, "Cannot set ca file: %s\n",
				tls_config_error(cfg));
		tls_config_free(cfg);
		return EXIT_FAILURE;
	}
	*/

	if (tls_config_set_keypair_file(cfg, "/tmp/my.crt", "/tmp/my.key") < 0)
	{
		fprintf(stderr, "tls_config_set_keypair_file(): %s\n",
				tls_config_error(cfg));
		tls_config_free(cfg);
		return EXIT_FAILURE;
	}

	if ((tls = tls_server()) == NULL)
	{
		fputs("tls_server() failed\n", stderr);
		tls_config_free(cfg);
		return EXIT_FAILURE;
	}

	if (tls_configure(tls, cfg) < 0) {
		fprintf(stderr, "tls_configure(): %s\n", tls_error(tls));
		tls_free(tls);
		tls_config_free(cfg);
		return EXIT_FAILURE;
	}

	while (1)
	{
		int accepted;
		struct tls *accepted_tls;
		char msg[MAX_IRC_MSG];
		ssize_t amt_read;

		/* open socket */
		if ((accepted = accept(sock, NULL, NULL)) < 0)
		{
			perror("accept()");
			tls_free(tls);
			tls_config_free(cfg);
			return EXIT_FAILURE;
		}
		/* TLS handshake */
		if (tls_accept_socket(tls, &accepted_tls, sock) < 0)
		{
			fprintf(stderr, "tls_accept_socket(): %s\n", tls_error(tls));
			close(accepted);
			continue;
		}

		while ((amt_read = tls_read(accepted_tls, msg, MAX_IRC_MSG)) > 0)
			printf("CLIENT: %s\n", msg);

		if (amt_read < 0)
			fprintf(stderr, "tls_read(): %s\n", tls_error(accepted_tls));

		tls_close(accepted_tls);
		tls_free(accepted_tls);
		close(accepted);
	}

	/* should not get here */
	close(sock);
	tls_free(tls);
	tls_config_free(cfg);
	return EXIT_SUCCESS;
}
