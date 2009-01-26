/* $Id$ */

#include <sys/param.h>

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>

#include "psc_util/log.h"
#include "psc_util/prsig.h"
#include "psc_util/strlcat.h"

extern char *sys_sigabbrev[];

void
psc_sigappend(char buf[LINE_MAX], const char *str)
{
	if (buf[0] != '\0')
		psc_strlcat(buf, ",", sizeof(buf));
	psc_strlcat(buf, str, sizeof(buf));
}

#define PNSIG 32

void
psc_prsig(void)
{
	struct sigaction sa;
	char buf[LINE_MAX];
	uint64_t mask;
	int i, j;

	i = printf("%3s %-6s %-16s %-7s", "sig", "name", "block mask", "action");
	putchar('\n');
	while (i--)
		putchar('=');
	putchar('\n');
	for (i = 1; i < PNSIG; i++) {
		if (sigaction(i, NULL, &sa) == -1)
			continue;

		buf[0] = '\0';
		if (sa.sa_handler == SIG_DFL)
			psc_sigappend(buf, "default");
		else if (sa.sa_handler == SIG_IGN)
			psc_sigappend(buf, "ignored");
		else if (sa.sa_handler == NULL)
			psc_sigappend(buf, "zero?");
		else
			psc_sigappend(buf, "caught");

		mask = 0;
		for (j = 1; j < PNSIG; j++)
			if (sigismember(&sa.sa_mask, j))
				mask |= 1 << (j - 1);
		printf("%3d %-6s %016"PRIx64" %s\n",
		    i, sys_sigabbrev[i], mask, buf);
	}
}
