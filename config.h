#ifndef CONFIG_H
#define CONFIG_H

#define MAX_CONFIG_LINE 512

struct main_config
{
	char local_host[MAX_CONFIG_LINE];
	char local_port[MAX_CONFIG_LINE];
	char local_user[MAX_CONFIG_LINE];
	char local_pass[MAX_CONFIG_LINE];

	char host[MAX_CONFIG_LINE];
	char port[MAX_CONFIG_LINE];
	char nick[MAX_CONFIG_LINE];
	char pass[MAX_CONFIG_LINE];
};

struct main_config *load_config(const char *path);

#endif
