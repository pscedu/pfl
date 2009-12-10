/* $Id$ */

/*
 * Subsystem definitions.
 */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <strings.h>

#include "psc_ds/dynarray.h"
#include "psc_util/alloc.h"
#include "psc_util/log.h"
#include "psc_util/subsys.h"

struct psc_subsys {
	const char	*pss_name;
	int		 pss_loglevel;
};

struct psc_dynarray	psc_subsystems = DYNARRAY_INIT;
int			psc_nsubsys;

int
psc_subsys_id(const char *name)
{
	const struct psc_subsys **ss;
	int n, len;

	ss = dynarray_get(&psc_subsystems);
	len = dynarray_len(&psc_subsystems);
	for (n = 0; n < len; n++)
		if (strcasecmp(name, ss[n]->pss_name) == 0)
			return (n);
	return (-1);
}

const char *
psc_subsys_name(int ssid)
{
	const struct psc_subsys *ss;

	if (ssid < 0 || ssid >= psc_nsubsys)
		return ("<unknown>");
	ss = dynarray_getpos(&psc_subsystems, ssid);
	return (ss->pss_name);
}

void
psc_subsys_register(int ssid, const char *name)
{
	char *p, *endp, buf[BUFSIZ];
	struct psc_subsys *ss;
	long l;

	ss = psc_alloc(sizeof(*ss), PAF_NOLOG);
	ss->pss_name = name;

	snprintf(buf, sizeof(buf), "PSC_LOG_LEVEL_%s", name);
	p = getenv(buf);
	if (p) {
		errno = 0;
		endp = NULL;
		l = strtol(p, &endp, 10);
		if (endp == p || *endp != '\0' ||
		    l < 0 || l >= PNLOGLEVELS)
			err(1, "invalid log level env: %s", p);
		ss->pss_loglevel = (int)l;
	} else
		ss->pss_loglevel = psc_log_getlevel_global();

	dynarray_add(&psc_subsystems, ss);
	if (psc_nsubsys++ != ssid)
		psc_fatalx("bad ID %d for subsys %s [want %d], "
		    "check order", ssid, name, psc_nsubsys);
}

int
psc_log_getlevel_ss(int ssid)
{
	const struct psc_subsys *ss;

	if (ssid >= psc_nsubsys || ssid < 0)
		/* must use errx(3) here to avoid loops with psclog */
		errx(1, "subsystem out of bounds (%d)", ssid);
	ss = dynarray_getpos(&psc_subsystems, ssid);
	return (ss->pss_loglevel);
}

void
psc_log_setlevel_ss(int ssid, int newlevel)
{
	struct psc_subsys **ss;
	int i;

	if (newlevel >= PNLOGLEVELS || newlevel < 0)
		psc_fatalx("log level out of bounds (%d)", newlevel);

	ss = dynarray_get(&psc_subsystems);

	if (ssid == PSS_ALL)
		for (i = 0; i < psc_nsubsys; i++)
			ss[i]->pss_loglevel = newlevel;
	else if (ssid >= psc_nsubsys || ssid < 0)
		psc_fatalx("subsystem out of bounds (%d)", ssid);
	else
		ss[ssid]->pss_loglevel = newlevel;
}
