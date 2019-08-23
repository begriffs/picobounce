#ifndef CONFIG_H
#define CONFIG_H

struct irc_network
{
	char *host;
	int port;
	char *nick;
	char *pass;
};

struct irc_network *load_config(const char *path);
void free_config(struct irc_network *net);

#endif
