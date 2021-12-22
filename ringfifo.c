#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#include <derp/list.h>

char            *name_in, *name_out;
list            *lines;
pthread_mutex_t  mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t   ready = PTHREAD_COND_INITIALIZER;

#define DEFAULT_MAX_LINES 8192

void *write_lines(void *unused)
{
	(void)unused;

	/* when fifo readers disappear, don't hit us with
	 * a signal. We'll check for EPIPE from write(3) */
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &sigs, NULL);

	while (1)
	{
		int fd;
		if ((fd = open(name_out, O_WRONLY)) < 0)
		{
			perror("open");
			exit(EXIT_FAILURE);
		}

		while (1)
		{
			char *line;

			pthread_mutex_lock(&mutex);
			while (l_length(lines) < 1)
				pthread_cond_wait(&ready, &mutex);
			line = l_remove_first(lines);
			pthread_mutex_unlock(&mutex);

			if (write(fd, line, strlen(line)) < 0)
			{
				/* reader of name.out hung up on us */
				if (errno == EPIPE)
				{
					pthread_mutex_lock(&mutex);
					/* Put line back, we couldn't send it.
					 *
					 * TODO: ensure it doesn't get out of order, and
					 * also that this doesn't throw off our max */
					l_prepend(lines, line);
					pthread_mutex_unlock(&mutex);
					/* go back to reopen fd */
					close(fd);
					break;
				}
				else
				{
					perror("write");
					exit(EXIT_FAILURE);
				}
			}
			else
				free(line);
		}
	}
	return NULL;
}

int main(int argc, char **argv)
{
	size_t maxlen = argc < 3
		? DEFAULT_MAX_LINES
		: strtol(argv[2], NULL, 10);
	lines = l_new();

	if (argc < 2)
	{
		fprintf(stderr, "Usage %s name [maxlines]\n", *argv);
		return EXIT_FAILURE;
	}

	name_in  = malloc(strlen(argv[1]) + 5);
	name_out = malloc(strlen(argv[1]) + 5);
	strcpy(name_in, argv[1]);  strcat(name_in, ".in");
	strcpy(name_out, argv[1]); strcat(name_out, ".out");

	if (mkfifo(name_in,  S_IWUSR | S_IRUSR) != 0 ||
	    mkfifo(name_out, S_IWUSR | S_IRUSR) != 0)
	{
		perror("mkfifo");
		return EXIT_FAILURE;
	}

	pthread_t writer;
	pthread_create(&writer, NULL, &write_lines, NULL);

	while (1)
	{
		FILE *in;
		char line[BUFSIZ+1];

		if ((in = fopen(name_in, "r")) == NULL)
		{
			perror("fopen");
			return EXIT_FAILURE;
		}
		setvbuf(in, NULL, _IOLBF, 0);
		while (fgets(line, BUFSIZ, in))
		{
			pthread_mutex_lock(&mutex);
			if (maxlen != 0 && 1+l_length(lines) > maxlen)
				free(l_remove_first(lines));
			l_append(lines, strdup(line));
			pthread_cond_signal(&ready);
			pthread_mutex_unlock(&mutex);
		}
		/* the fifo writer(s) to name.in all closed their
		 * file descriptors, so we'll close and reopen */
		fclose(in);
	}

	/* should never get here */
	return EXIT_SUCCESS;
}
