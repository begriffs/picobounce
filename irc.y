%define parse.error verbose
%define api.pure true
%define api.prefix {ircv3_}

%define parse.trace

%code top {
	#include <stdio.h>
	#include <stdlib.h>
}

%code requires {
	#include "irc.h"
}

%code {
    int ircv3_error(void *, char const *, const void *);
    int ircv3_lex(void *lval, const void *);
}

%union
{
	char *str;
	struct prefix *prefix;
	treemap *map;
	struct map_pair *pair;
	list *list;
	struct irc_message *msg;
}

%parse-param {struct irc_message **result}
%param {void *scanner}

%token          SPACE
%token <str>    COMMAND MIDDLE TRAILING
%token <pair>   TAG
%token <prefix> PREFIX

%type <msg> message tagged_message prefixed_message
%type <map> tags
%type <list> params

%%

final :
  tagged_message { *result = $1; }
;

tagged_message :
  '@' tags[ts] SPACE prefixed_message[msg] {
	$msg->tags = $ts;
	$$ = $msg;
  }
| prefixed_message
;

prefixed_message :
  ':' PREFIX[pfx] SPACE message[msg] {
	$msg->prefix = $pfx;
	$$ = $msg;
  }
| message
;

message :
  COMMAND[cmd] SPACE params[ps] {
	struct irc_message *m = malloc(sizeof *m);
	*m = (struct irc_message) {
		.command=$cmd, .params=$ps
	};
	$$ = m;
  }
;

tags :
  TAG {
	treemap *t = tm_new(derp_strcmp, NULL);
	tm_dtor(t, derp_free, derp_free, NULL);
	tm_insert(t, $1->k, $1->v);
	$$ = t;
  }
| tags[ts] ';' TAG[t] {
	tm_insert($ts, $t->k, $t->v);
	$$ = $ts;
  }
;

params :
  TRAILING {
	$$ = l_new();
	l_dtor($$, derp_free, NULL);
	l_prepend($$, $1);
  }
| MIDDLE[mid] SPACE params[ps] {
	l_prepend($ps, $mid);
	$$ = $ps;
  }
| %empty {
	$$ = l_new();
	l_dtor($$, derp_free, NULL);
  }
;

%%

int ircv3_error(void *yylval, char const *msg, const void *s)
{
	(void)yylval;
	(void)s;
	return fprintf(stderr, "IRCv3 ERROR: %s\n", msg);
}
