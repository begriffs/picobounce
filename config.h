#ifndef CONFIG_H
#define CONFIG_H

struct irc_network
{
	char *local_host;
	char *local_port;
	char *local_user;
	char *local_pass;

	char *host;
	char *port;
	char *nick;
	char *pass;
};

struct irc_network *load_config(const char *path);
void irc_network_config_free(struct irc_network *net);

#endif
