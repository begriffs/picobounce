#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <derp/list.h>
#include <pthread.h>

list            *lines;
bool             reading = true;
pthread_mutex_t  mutex   = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t   ready   = PTHREAD_COND_INITIALIZER;

void *print_queue(void *unused)
{
	(void)unused;
	char *line;
	bool loop = true;
	while (loop)
	{
		pthread_mutex_lock(&mutex);
		while (l_length(lines) < 1 && reading)
			pthread_cond_wait(&ready, &mutex);
		if (!reading)
		{
			line = NULL;
			loop = false;
		}
		else
			line = l_remove_first(lines);
		pthread_mutex_unlock(&mutex);

		if (line)
		{
			fputs(line, stdout);
			free(line);
		}
	}

	/* drain the rest */
	while ((line = l_remove_first(lines)) != NULL)
	{
		fputs(line, stdout);
		free(line);
	}
	return NULL;
}

int main(int argc, char **argv)
{
	size_t maxlen = argc < 2
		? 1024*1024
		: strtol(argv[1], NULL, 10);
	lines = l_new();
	//l_dtor(lines, derp_free, NULL);

	pthread_t writer;
	pthread_create(&writer, NULL, &print_queue, NULL);

	setvbuf(stdin,  NULL, _IOLBF, 0);
	setvbuf(stdout, NULL, _IOLBF, 0);

	char *line;
	size_t lim = 0;
	while (line = NULL, getline(&line, &lim, stdin) > 0)
	{
		pthread_mutex_lock(&mutex);
		if (maxlen != 0 && 1+l_length(lines) > maxlen)
			free(l_remove_first(lines));
		l_append(lines, line);
		pthread_cond_signal(&ready);
		pthread_mutex_unlock(&mutex);
	}

	/* writer can quit when queue empty */
	pthread_mutex_lock(&mutex);
	reading = false;
	pthread_cond_signal(&ready);
	pthread_mutex_unlock(&mutex);

	pthread_join(writer, NULL);
	return EXIT_SUCCESS;
}
