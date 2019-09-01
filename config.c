#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define ARR_LEN(arr) (sizeof(arr)/sizeof((arr)[0]))

struct main_config *load_config(const char *path)
{
	FILE *f;
	char *val;
	char line[MAX_CONFIG_LINE];
	struct main_config *net;

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
	*net = (struct main_config){0};

	errno = 0;
	while (fgets(line, ARR_LEN(line), f))
	{
		const struct { char *name; char (*dest)[MAX_CONFIG_LINE]; } opts[] =
		{
			{"local_host", &net->local_host},
			{"local_port", &net->local_port},
			{"local_user", &net->local_user},
			{"local_pass", &net->local_pass},
			{"host", &net->host}, {"nick", &net->nick},
			{"pass", &net->pass}
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

		for (i = 0; i < ARR_LEN(opts); i++)
			if (strcmp(line, opts[i].name) == 0)
			{
				strcpy(*opts[i].dest, val);
				break;
			}
		if (i == ARR_LEN(opts))
		{
			/* odd one out -- all the others are strings */
			if (strcmp(line, "port") == 0)
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
			else
				fprintf(stderr, "Unknown config key: %s\n", line);
		}
	}
	if (ferror(f))
		perror("load_config");

	return net;
}
