#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <regex.h>
#include <sys/socket.h>

#include <tls.h>

#include "config.h"
#include "sasl.h"
#include "window.h"
#include "messages.h"

#define TCP_BACKLOG  SOMAXCONN

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

static ssize_t tls_printf(struct tls *tls, const char *fmt, ...)
{
	va_list ap;
	ssize_t ret;
	char out[MAX_IRC_MSG+1];

	va_start(ap, fmt);

	if (vsnprintf(out, MAX_IRC_MSG, fmt, ap) > MAX_IRC_MSG)
		fprintf(stderr, "tls_printf(): message truncated: %s\n", fmt);

	if ((ret = tls_write(tls, out, strlen(out))) < 0)
		fprintf(stderr, "tls_write(): %s\n", tls_error(tls));

	printf("<- %s", out);

	va_end(ap);
	return ret;
}

void client_session(struct tls *tls, const char *local_user, const char *local_pass)
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
					tls_printf(tls,
							":localhost 906 %s :SASL authentication aborted\n", nick);
				else
				{
					char username[MAX_SASL_FIELD],
					     password[MAX_SASL_FIELD];
					extract_creds(auth, username, password);
					if (strcmp(local_user, username) == 0 ||
							strcmp(local_pass, password) == 0)
						tls_printf(tls,
								":localhost 903 %s :SASL authentication successful\n", nick);
					else
						tls_printf(tls,
								":localhost 904 %s :SASL authentication failed\n", nick);
				}
			}
		}
	}

	if (amt_read < 0)
		fprintf(stderr, "tls_read(): %s\n", tls_error(tls));

	window_free(w);
}

/* if this fails it calls exit, not pthread_exit because there's
 * no reason to live if you're blocked from ever accepting clients
 */
void handle_clients(struct main_config *cfg)
{
	int sock;
	struct tls_config *tls_cfg;
	struct tls *tls;

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

		client_session(accepted_tls, cfg->local_user, cfg->local_pass);

		tls_close(accepted_tls);
		tls_free(accepted_tls);
		close(accepted);
	}

	/* should not get here */
	close(sock);
}

bool irc_sasl_auth(struct tls *tls, const char *user, const char *pass)
{
	regex_t sasl_supported, sasl_unsupported, sasl_ok, sasl_bad;
	bool authed = false;
	char msg[MAX_IRC_MSG+1];
	ssize_t amt_read;
	window *w;

	if(regcomp(&sasl_supported, " ACK :sasl", 0) != 0 ||
	   regcomp(&sasl_unsupported, " NACK :sasl", 0) != 0 ||
	   regcomp(&sasl_ok, "^:([^\\s])+ 903 ", REG_EXTENDED) != 0 ||
	   regcomp(&sasl_bad, "^:([^\\s])+ 904 ", REG_EXTENDED) != 0)
	{
		fputs("Failed to compile regex\n", stderr);
		return false;
	}

	if (!(w = window_alloc(MAX_IRC_MSG)))
	{
		fputs("Failed to allocate irc message buffer\n", stderr);
		return false;
	}

	tls_printf(tls, "CAP REQ :sasl\n");
	tls_printf(tls, "NICK %s\n", user);

	while ((amt_read = tls_read(tls, msg, MAX_IRC_MSG)) > 0)
	{
		char *line;

		msg[amt_read] = '\0';
		window_fill(w, msg);
		while ((line = window_next(w)) != NULL)
		{
			printf("s> %s\n", line);
			if (regexec(&sasl_supported, line, 0, NULL, 0) == 0)
				tls_printf(tls, "AUTHENTICATE PLAIN\n");
			else if (regexec(&sasl_unsupported, line, 0, NULL, 0) == 0)
				goto irc_sasl_auth_done;
			else if (strcmp(line, "AUTHENTICATE +") == 0)
				tls_printf(tls, "AUTHENTICATE %s\n", encode_creds(user, pass));
			else if (regexec(&sasl_ok, line, 0, NULL, 0) == 0)
			{
				authed = true;
				goto irc_sasl_auth_done;
			}
			else if (regexec(&sasl_bad, line, 0, NULL, 0) == 0)
				goto irc_sasl_auth_done;
		}
	}

irc_sasl_auth_done:

	regfree(&sasl_supported);
	regfree(&sasl_unsupported);
	regfree(&sasl_ok);
	regfree(&sasl_bad);
	window_free(w);
	return authed;
}

int main(int argc, const char **argv)
{
	struct main_config *cfg;
	pthread_t client_thread;
	struct tls *tls;

	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s path-to-config\n", *argv);
		return EXIT_FAILURE;
	}

	if (!(cfg = load_config(argv[1])))
	{
		fprintf(stderr, "Failed to load config from \"%s\"\n", argv[1]);
		return EXIT_FAILURE;
	}

	printf("Connecting to %s:%s as %s\n", cfg->host, cfg->port, cfg->nick);

	if (!(tls = tls_client()))
	{
		fputs("Failed to obtain TLS client\n", stderr);
		return EXIT_FAILURE;
	}
	if (tls_connect(tls, cfg->host, cfg->port) != 0)
	{
		fprintf(stderr, "tls_connect(): %s\n", tls_error(tls));
		tls_free(tls);
		return EXIT_FAILURE;
	}
	if (!irc_sasl_auth(tls, cfg->nick, cfg->pass))
	{
		fputs("Server authentication failed\n", stderr);
		tls_close(tls);
		tls_free(tls);
		return EXIT_FAILURE;
	}

	puts("We did it lads.");

	pthread_create(&client_thread, NULL, (void (*))(void *)&handle_clients, cfg);

	tls_close(tls);
	tls_free(tls);
	return EXIT_SUCCESS;
}
