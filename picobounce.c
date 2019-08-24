#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <tls.h>

#include "config.h"
#include "sasl.h"
#include "window.h"

#define TCP_BACKLOG  SOMAXCONN
#define MAX_IRC_MSG  512
#define MAX_IRC_NICK 9

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

/* wrap tls_write with formatting and error checking */
ssize_t irc_printf(struct tls *tls, const char *fmt, ...)
{
	va_list ap;
	ssize_t ret;
	char out[MAX_IRC_MSG+1];

	va_start(ap, fmt);

	if (vsnprintf(out, MAX_IRC_MSG, fmt, ap) > MAX_IRC_MSG)
		fprintf(stderr, "irc_printf(): message truncated: %s\n", fmt);

	if ((ret = tls_write(tls, out, strlen(out))) < 0)
		fprintf(stderr, "tls_write(): %s\n", tls_error(tls));

	printf("<- %s", out);

	va_end(ap);
	return ret;
}

bool is_login_correct(const char *user, const char *pass)
{
	return (strcmp(user, "mrfun") == 0) && (strcmp(pass, "fun") == 0);
}

void irc_session(struct tls *tls)
{
	char msg[MAX_IRC_MSG+1],
	     nick[MAX_IRC_NICK+1] = "*";
	ssize_t amt_read;
	window *w;

	if (!(w = window_alloc(MAX_IRC_MSG)))
	{
		fputs("Failed to allocate irc message buffer\n", stderr);
		return;
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
			else if (strncmp(line, "CAP REQ :", 9) == 0)
			{
				char *cap = line+9;
				irc_printf(tls, ":localhost CAP %s %s :%s\n",
						nick, strcmp(cap, "sasl") ? "NAK" : "ACK", cap);
			}
			else if (strncmp(line, "CAP LS 302", 8) == 0)
				irc_printf(tls, ":localhost CAP %s LS :sasl\n", nick);
			else if (strncmp(line, "AUTHENTICATE ", 13) == 0)
			{
				char *auth = line+13;
				if (strcmp(auth, "PLAIN") == 0)
					irc_printf(tls, "AUTHENTICATE +\n");
				else if (strcmp(auth, "*") == 0)
					irc_printf(tls,
							":localhost 906 %s :SASL authentication aborted\n", nick);
				else
				{
					char username[MAX_SASL_FIELD],
					     password[MAX_SASL_FIELD];
					extract_creds(auth, username, password);
					if (is_login_correct(username, password))
						irc_printf(tls,
								":localhost 903 %s :SASL authentication successful\n", nick);
					else
						irc_printf(tls,
								":localhost 904 %s :SASL authentication failed\n", nick);
				}
			}
		}
	}

	if (amt_read < 0)
		fprintf(stderr, "tls_read(): %s\n", tls_error(tls));

	window_free(w);
}

/* see
 * https://github.com/bob-beck/libtls/blob/master/TUTORIAL.md#basic-libtls-use
 * and
 * https://github.com/OSUSecLab/Stacco/blob/b4a598c3061537f630526a07545e276cccf298cb/test/libressl/server.c
 */
int main(int argc, const char **argv)
{
	int sock;
	struct tls_config *cfg;
	struct tls *tls;
	struct irc_network *net;

	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s path-to-config\n", *argv);
		return EXIT_FAILURE;
	}
	if (!(net = load_config(argv[1])))
	{
		fprintf(stderr, "Failed to load config from \"%s\"\n", argv[1]);
		return EXIT_FAILURE;
	}

	free_config(net);

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

	if (tls_config_set_keypair_file(cfg, "my.crt", "my.key") < 0)
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

		/* open socket */
		if ((accepted = accept(sock, NULL, NULL)) < 0)
		{
			perror("accept()");
			tls_free(tls);
			tls_config_free(cfg);
			return EXIT_FAILURE;
		}
		/* TLS handshake */
		if (tls_accept_socket(tls, &accepted_tls, accepted) < 0)
		{
			fprintf(stderr, "tls_accept_socket(): %s\n", tls_error(tls));
			close(accepted);
			continue;
		}

		irc_session(accepted_tls);

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
