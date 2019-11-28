#include "client.h"
#include "messages.h"
#include "upstream.h"
#include "window.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

struct msg_log *g_from_client, *g_to_client;

void *client_write(struct tls *tls)
{
	while (1)
	{
		struct msg *m = msg_log_consume(g_from_upstream);

		if (tls_write(tls, m->text, strlen(m->text)) < 0)
		{
			fprintf(stderr, "Error relaying to client: tls_write(): %s\n",
					tls_error(tls));
			/* slight chance this is slightly out of order now, but no worries */
			msg_log_putback(g_from_upstream, m);
			/* client probably disconnected, return so that parent
			 * can respawn us after reconnection */
			return NULL;
		}
	}
	return NULL;
}

/* adapted from
 * http://pubs.opengroup.org/onlinepubs/9699919799/functions/getaddrinfo.html
 *
 * svc: either a name like "ftp" or a port number as string
 *
 * Returns: socket file desciptor, or negative error value
 */
static int negotiate_listen(const char *svc)
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

static bool
client_auth(struct tls *tls, const char *local_user, const char *local_pass)
{
	char msg[MAX_IRC_MSG+1],
	     nick[MAX_IRC_NICK+1] = "*";
	ssize_t amt_read;
	window *w;
	bool authed = false;

	if (!(w = window_alloc(MAX_IRC_MSG)))
	{
		fputs("Failed to allocate irc message buffer\n", stderr);
		return false;
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
				tls_printf(tls, ":localhost CAP %s %s :%s\n",
						nick, strcmp(cap, "sasl") ? "NAK" : "ACK", cap);
			}
			else if (strncmp(line, "CAP LS 302", 8) == 0)
				tls_printf(tls, ":localhost CAP %s LS :sasl\n", nick);
			else if (strncmp(line, "AUTHENTICATE ", 13) == 0)
			{
				char *auth = line+13;
				if (strcmp(auth, "PLAIN") == 0)
					tls_printf(tls, "AUTHENTICATE +\n");
				else if (strcmp(auth, "*") == 0)
				{
					tls_printf(tls,
							":localhost 906 %s :SASL authentication aborted\n", nick);
					break;
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
						authed = true;
						break;
					}
					else
					{
						tls_printf(tls,
								":localhost 904 %s :SASL authentication failed\n", nick);
						break;
					}
				}
			}
		}
	}

	if (amt_read < 0)
		fprintf(stderr, "tls_read(): %s\n", tls_error(tls));

	window_free(w);
	return authed;
}

/* if this fails it calls exit, not pthread_exit because there's
 * no reason to live if you're blocked from ever accepting clients
 */
void client_read(struct main_config *cfg)
{
	int sock;
	struct tls_config *tls_cfg;
	struct tls *tls;
	pthread_t client_write_thread;

	if ((sock = negotiate_listen(cfg->local_port)) < 0)
	{
		fprintf(stderr, "Unable to listen on port \"%s\"\n", cfg->local_port);
		exit(EXIT_FAILURE);
	}


	if ((tls_cfg = tls_config_new()) == NULL)
	{
		fputs("tls_config_new() failed\n", stderr);
		exit(EXIT_FAILURE);
	}

	if (tls_config_set_keypair_file(tls_cfg, "my.crt", "my.key") < 0)
	{
		fprintf(stderr, "tls_config_set_keypair_file(): %s\n",
				tls_config_error(tls_cfg));
		exit(EXIT_FAILURE);
	}

	if ((tls = tls_server()) == NULL)
	{
		fputs("tls_server() failed\n", stderr);
		exit(EXIT_FAILURE);
	}

	if (tls_configure(tls, tls_cfg) < 0) {
		fprintf(stderr, "tls_configure(): %s\n", tls_error(tls));
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		int accepted;
		struct tls *accepted_tls;

		/* open socket */
		if ((accepted = accept(sock, NULL, NULL)) < 0)
		{
			perror("accept()");
			exit(EXIT_FAILURE);
		}
		/* TLS handshake */
		if (tls_accept_socket(tls, &accepted_tls, accepted) < 0)
		{
			fprintf(stderr, "tls_accept_socket(): %s\n", tls_error(tls));
			close(accepted);
			continue;
		}

		if (client_auth(accepted_tls, cfg->local_user, cfg->local_pass))
		{
			pthread_create(&client_write_thread, NULL,
					(void (*))(void *)&client_write, accepted_tls);

		}

		tls_close(accepted_tls);
		tls_free(accepted_tls);
		close(accepted);
	}

	/* should not get here */
	close(sock);
}
