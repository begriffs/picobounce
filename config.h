#ifndef CONFIG_H
#define CONFIG_H

struct irc_network
{
	char *host;
	char *port;
	char *nick;
	char *pass;
};

struct irc_network *load_config(const char *path);
void irc_network_config_free(struct irc_network *net);

#endif
