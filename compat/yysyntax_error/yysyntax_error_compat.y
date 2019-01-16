/* $Id$ */

%{
#define YYSTYPE char *

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pfl/log.h"

void		 yyerror(const char *, ...);
int		 yylex(void);

#undef psclog_trace
#define psclog_trace(fmt, ...)

#undef PFL_RETURNX
#define PFL_RETURNX()		return

#undef PFL_RETURN
#define PFL_RETURN(rc)		return (rc)

#undef PFL_RETURN_LIT
#define PFL_RETURN_LIT(s)	return (s)
%}

%start config

%%

config		:
		;

%%

int
yylex(void)
{
	return (0);
}

int
main(int argc, char *argv[])
{
	yyparse();
	(void)argc;
	(void)argv;
	exit(0);
}

void
yyerror(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verrx(1, fmt, ap);
	va_end(ap);
}
