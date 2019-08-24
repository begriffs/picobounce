#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

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
	while (fgets(line, sizeof line, f))
	{
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
			fprintf(stderr, "Failed to allocate config value\n");
			fclose(f);
			free_config(net);
			return NULL;
		}

		if (strcmp(line, "host") == 0)
			net->host = val_copy;
		else if (strcmp(line, "port") == 0)
		{
			char *end;
			long p = strtol(val, &end, 10);
			if (*val == '\0' || *end != '\0')
				fprintf(stderr, "Not a valid port number: \"%s\"\n", val);
			else if (p < 0 || p >= 65535)
				fprintf(stderr, "Port number out of range: %ld\n", p);
			else
				net->port = p;
		}
		else if (strcmp(line, "nick") == 0)
			net->nick = val_copy;
		else if (strcmp(line, "pass") == 0)
			net->pass = val_copy;
		else
		{
			free(val_copy);
			fprintf(stderr, "Unknown config key: %s\n", line);
		}
	}
	if (ferror(f))
		perror("load_config");

	return net;
}

void free_config(struct irc_network *net)
{
	if (!net)
		return;
	if (net->host) free(net->host);
	if (net->nick) free(net->nick);
	if (net->pass) free(net->pass);
	free(net);
}
