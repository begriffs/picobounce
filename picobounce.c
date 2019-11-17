#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include "config.h"
#include "upstream.h"
#include "client.h"

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

	g_from_upstream = msg_log_alloc();
	g_to_upstream = msg_log_alloc();
	if (!g_from_upstream || !g_to_upstream)
	{
		fprintf(stderr, "Unable to allocate message logs\n");
		return EXIT_FAILURE;
	}

	pthread_create(&upstream_read_thread, NULL,
			(void (*))(void *)&upstream_read, cfg);
	client_read(cfg);

	/* should not get here */
	return EXIT_SUCCESS;
}
