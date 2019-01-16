/* $Id$ */

%{
#include <stdio.h>
#include <string.h>

#include "pfl/log.h"

#undef psclog_trace
#define psclog_trace(fmt, ...)

#undef PFL_RETURNX
#define PFL_RETURNX()		return

#undef PFL_RETURN
#define PFL_RETURN(rc)		return (rc)

#undef PFL_RETURN_LIT
#define PFL_RETURN_LIT(s)	return (s)

#undef PFL_RETURN_STR
#define PFL_RETURN_STR(s)	return (s)
%}

%%

.		{ }

%%

int
yywrap(void)
{
	return (1);
}

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	exit(0);
}
