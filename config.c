#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define ARR_LEN(arr) (sizeof(arr)/sizeof((arr)[0]))

struct irc_network *load_config(const char *path)
{
	FILE *f;
	char *val, *val_copy;
	char line[512];
	struct irc_network *net;

	errno = 0;
   	if (!(f = fopen(path, "r")))
	{
		perror("load_config");
		return NULL;
	}

	if (!(net = malloc(sizeof *net)))
	{
		perror("load_config");
		fclose(f);
		return NULL;
	}
	*net = (struct irc_network){0};

	errno = 0;
	while (fgets(line, ARR_LEN(line), f))
	{
		const struct { char *name; char **dest; } opts[] =
		{
			{"local_host", &net->local_host},
			{"local_port", &net->local_port},
			{"local_user", &net->local_user},
			{"local_pass", &net->local_pass},
			{"host", &net->host}, {"port", &net->port},
			{"nick", &net->nick}, {"pass", &net->pass}
		};
		size_t i;

		line[strcspn(line, "\n")] = '\0';
		val = strchr(line, '=');
		if (!val)
		{
			fprintf(stderr, "Ignoring non key-value pair: %s\n", line);
			continue;
		}

		/* end string at key, and check */
		*val++ = '\0';
		if (!(val_copy = strdup(val)))
		{
			fprintf(stderr,
				"Failed to allocate config value for %s\n", line);
			fclose(f);
			irc_network_config_free(net);
			return NULL;
		}

		for (i = 0; i < ARR_LEN(opts); i++)
			if (strcmp(line, opts[i].name) == 0)
			{
				*opts[i].dest = val_copy;
				break;
			}
		if (i == ARR_LEN(opts))
		{
			free(val_copy);
			fprintf(stderr, "Unknown config key: %s\n", line);
		}
	}
	if (ferror(f))
		perror("load_config");

	return net;
}

void irc_network_config_free(struct irc_network *net)
{
	if (!net)
		return;
	if (net->host) free(net->host);
	if (net->nick) free(net->nick);
	if (net->pass) free(net->pass);
	free(net);
}
