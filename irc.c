#include "irc.tab.h"
#include "irc.lex.h"

#include <derp/treemap.h>
#include <derp/list.h>

struct irc_message *message_read(FILE *f)
{
	struct irc_message *m = NULL;
	yyscan_t scanner;

	if (yylex_init(&scanner) != 0)
		return NULL;
	yyset_in(f, scanner);

	yyparse(&m, scanner);
	yylex_destroy(scanner);
	return m;
}

void message_print(struct irc_message *m, FILE *f)
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
			fprintf(f, " %s\n", (char*)li->data);
	fputc('\n', f);
}
