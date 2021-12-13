#include <assert.h>
#include <stdio.h>

/* https://ircv3.net/specs/extensions/message-tags.html#size-limit */
#define MAX_IRC_MSG  8191

#define YYSTYPE IRCV3_STYPE
#include "irc.tab.h"
#include "irc.lex.h"
#include "irc.h"

struct irc_message *irc_message_read(FILE *f)
{
	struct irc_message *m = NULL;
	char *line = NULL;
	size_t len = MAX_IRC_MSG;
	yyscan_t scanner;
    YY_BUFFER_STATE buf;

	if (ircv3_lex_init(&scanner) != 0)
		return NULL;
	if (getline(&line, &len, f) < 0)
	{
		ircv3_lex_destroy(scanner);
		return NULL;
	}
    buf = ircv3__scan_string(line, scanner);

	ircv3_parse(&m, scanner);

    ircv3__delete_buffer(buf, scanner);
	ircv3_lex_destroy(scanner);
	free(line);
	return m;
}

void irc_message_print(struct irc_message *m, FILE *f)
{
	assert(m);
	if (m->tags)
	{
		struct tm_iter  *it = tm_iter_begin(m->tags);
		struct map_pair *p;

		fputc('@', f);
		bool first = true;
		while ((p = tm_iter_next(it)) != NULL)
		{
			if (!first)
				fputc(';', f);
			else
				first = false;

			fprintf(f, "%s=%s", (char*)p->k, (char*)p->v);
		}
		fputc(' ', f);
		tm_iter_free(it);
	}
	if (m->prefix)
	{
		fputc(':', f);
		if (m->prefix->nick)
		{
			fputs(m->prefix->nick, f);
			if (m->prefix->user)
				fprintf(f, "!%s", m->prefix->user);
			if (m->prefix->host)
				fprintf(f, "@%s", m->prefix->host);
		}
		else
			fputs(m->prefix->host, f);
		fputc(' ', f);
	}

	fputs(m->command, f);

	if (!l_is_empty(m->params))
		for (list_item *li = l_first(m->params); li; li = li->next)
			fprintf(f, "%s%s", li->next ? " " : " :", (char*)li->data);
	fputc('\n', f);
}

void irc_message_free(struct irc_message *m)
{
	if (!m)
		return;
	tm_free(m->tags);
	if (m->prefix)
	{
		free(m->prefix->host);
		free(m->prefix->nick);
		free(m->prefix->user);
		free(m->prefix);
	}
	free(m->command);
	l_free(m->params);
}
