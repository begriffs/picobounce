#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <regex.h>
#include <sys/socket.h>
#include <unistd.h>

#include <pthread.h>

#include "config.h"
#include "irc.h"
#include "messages.h"
#include "set.h"
#include "window.h"

#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)

struct msg_log *g_from_upstream, *g_from_client;
void *g_active_channels = EMPTY_SET;

void upstream_read(struct main_config *cfg);
void client_read(struct main_config *cfg);

int main(int argc, const char **argv)
{
	struct main_config *cfg;
	pthread_t upstream_read_thread;

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

	g_from_upstream = msg_log_alloc(cfg->max_messages);
	g_from_client = msg_log_alloc(NO_MESSAGE_LIMIT);
	if (!g_from_upstream || !g_from_client)
	{
		fputs("Unable to allocate message logs\n", stderr);
		return EXIT_FAILURE;
	}

	pthread_create(&upstream_read_thread, NULL,
			(void *(*)(void*))upstream_read, cfg);
	client_read(cfg);

	/* should not get here */
	return EXIT_SUCCESS;
}


static bool
upstream_auth(struct tls *tls, const char *user, const char *pass)
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
		goto irc_sasl_auth_done;
	}

	tls_printf(tls, "CAP REQ :sasl\n");
	tls_printf(tls, "NICK %s\n", user);
	tls_printf(tls, "USER singleuser localhost localhost :singleuser\n");

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
				tls_printf(tls, "CAP END\n");
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

static void *upstream_write(void *p)
{
	struct tls *tls = p;
	int oldstate;

	while (1)
	{
		struct msg *m = msg_log_consume(g_from_client);

		/* in case tls_write is a cancellation point */
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

		if (tls_write(tls, m->text, strlen(m->text)) < 0)
		{
			fprintf(stderr, "Error relaying to upstream: tls_write(): %s\n",
					tls_error(tls));
			/* a chance this is slightly out of order now, but no worries */
			msg_log_putback(g_from_client, m);
			pthread_setcancelstate(oldstate, &oldstate);
			/* upstream probably disconnected, so return and parent
			 * can respawn us after reconnection */
			return NULL;
		}
		tls_write(tls, "\n", 1);

		/* return to original cancellation state */
		pthread_setcancelstate(oldstate, &oldstate);
	}
	return NULL;
}

void upstream_read(struct main_config *cfg)
{
	struct tls *tls;
	window *w;
	pthread_t upstream_write_thread;
	char msg[MAX_IRC_MSG+1], arg1[MAX_IRC_MSG+1], arg2[MAX_IRC_MSG+1];
	ssize_t amt_read;

	if (!(w = window_alloc(MAX_IRC_MSG)))
	{
		fputs("Failed to allocate irc message buffer\n", stderr);
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		if (!(tls = tls_client()))
		{
			fputs("Failed to obtain TLS client\n", stderr);
			exit(EXIT_FAILURE);
		}

		printf("Connecting to %s:%s as %s\n", cfg->host, cfg->port, cfg->nick);

		if (tls_connect(tls, cfg->host, cfg->port) != 0)
		{
			fprintf(stderr, "tls_connect(): %s\n", tls_error(tls));
			tls_free(tls);
			exit(EXIT_FAILURE);
		}
		if (!upstream_auth(tls, cfg->nick, cfg->pass))
		{
			fputs("Server authentication failed\n", stderr);
			tls_close(tls);
			tls_free(tls);
			exit(EXIT_FAILURE);
		}
		printf("Authenticated with %s\n", cfg->host);

		pthread_create(&upstream_write_thread, NULL,
				upstream_write, tls);
		while ((amt_read = tls_read(tls, msg, MAX_IRC_MSG)) > 0)
		{
			char *line;
			struct msg *m;
			   
			msg[amt_read] = '\0';
			window_fill(w, msg);
			while ((line = window_next(w)) != NULL)
			{
				printf("s> %s\n", line);
				if (strncmp(line, "PING ", 5) == 0)
				{
					if (!(m	= msg_alloc()))
					{
						fputs("Unable to queue PONG message\n", stderr);
						continue;
					}
					m->at = time(NULL);
					snprintf(m->text, MAX_IRC_MSG, "PONG %s", line+5);
					msg_log_add(g_from_client, m);
				}
				else if (2 == sscanf(
							line,
							"KICK %" STR(MAX_IRC_MSG) "s %" STR(MAX_IRC_MSG) "s",
						    arg1, arg2)
						 && strcmp(cfg->nick, arg2) == 0)
					set_rm(g_active_channels, arg1);
				else
				{
					if (!(m	= msg_alloc()))
					{
						fputs("Unable to queue message from upstream", stderr);
						continue;
					}
					m->at = time(NULL);
					snprintf(m->text, MAX_IRC_MSG, "%s", line);
					msg_log_add(g_from_upstream, m);
				}
			}
		}
		pthread_cancel(upstream_write_thread);
		pthread_join(upstream_write_thread, NULL);
		tls_close(tls);
		tls_free(tls);
		set_free(g_active_channels);
		g_active_channels = EMPTY_SET;
	}
}

static void *client_write(void *p)
{
	struct tls *tls = p;
	char stamped_msg[MAX_IRC_MSG + 50];
	struct msg *m;
	int oldstate;

	while (1)
	{
		m = msg_log_consume(g_from_upstream);

		/* For now just assuming the client has server-time
		 * capability -- mine does! In the future honor their choice.
		 */
		strftime(stamped_msg, sizeof stamped_msg,
				"@time=%Y-%m-%dT%H:%M:%S%Z ", gmtime(&m->at));
		strncat(stamped_msg, m->text, MAX_IRC_MSG);

		/* in case tls_write is a cancellation point */
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

		if (tls_write(tls, stamped_msg, strlen(stamped_msg)) < 0)
		{
			fprintf(stderr, "Error relaying to client: tls_write(): %s\n",
					tls_error(tls));
			/* slight chance this is slightly out of order now, but no worries */
			msg_log_putback(g_from_upstream, m);
			/* client probably disconnected, return and parent
			 * can respawn us after reconnection */
			return NULL;
		}
		tls_write(tls, "\n", 1);

		/* return to original cancellation state */
		pthread_setcancelstate(oldstate, &oldstate);
	}
	return NULL;
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
	window *w;

	if (!(w = window_alloc(MAX_IRC_MSG)))
	{
		fputs("Failed to allocate irc message buffer\n", stderr);
		exit(EXIT_FAILURE);
	}

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
		char msg[MAX_IRC_MSG+1];
		ssize_t amt_read;
		struct irc_caps caps;

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

		caps = client_auth(accepted_tls, cfg->local_user, cfg->local_pass);
		if (!caps.error)
		{
			pthread_create(&client_write_thread, NULL,
					client_write, accepted_tls);

			while ((amt_read = tls_read(accepted_tls, msg, MAX_IRC_MSG)) > 0)
			{
				char *line;
				struct msg *m;

				msg[amt_read] = '\0';
				window_fill(w, msg);
				while ((line = window_next(w)) != NULL)
				{
					if (strncmp(line, "QUIT ", 5) == 0)
						continue; /* client may quit, but we don't */
					if (strncmp(line, "JOIN ", 5) == 0)
					{
						if (set_contains(g_active_channels, "foo"))
						{
							/* already joined, so get TOPIC and NAMES for client */
							if (!(m = msg_alloc()))
							{
								fputs("Unable to queue message from client", stderr);
								continue;
							}
							m->at = time(NULL);
							snprintf(m->text, MAX_IRC_MSG, "TOPIC %s", line+5);
							msg_log_add(g_from_client, m);

							if (!(m = msg_alloc()))
							{
								fputs("Unable to queue message from client", stderr);
								continue;
							}
							m->at = time(NULL);
							snprintf(m->text, MAX_IRC_MSG, "NAMES %s", line+5);
							continue;
						}
					}
					else if (!(m = msg_alloc()))
					{
						fputs("Unable to queue message from client", stderr);
						continue;
					}
					m->at = time(NULL);
					strcpy(m->text, line);
					printf("-> %s\n", line);
					msg_log_add(g_from_client, m);
				}
			}
		}

		pthread_cancel(client_write_thread);
		pthread_join(client_write_thread, NULL);
		tls_close(accepted_tls);
		tls_free(accepted_tls);
		close(accepted);
	}

	/* should not get here */
	window_free(w);
	close(sock);
}
