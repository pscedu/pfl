/* $Id$ */

/*
 * Logging routines.
 * Notes:
 *	(o) We cannot use psc_fatal() for fatal errors here.  Instead use err(3).
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/syscall.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "psc_util/alloc.h"
#include "psc_util/cdefs.h"
#include "psc_util/fmtstr.h"
#include "psc_util/log.h"

#ifndef PSC_LOG_FMT
#define PSC_LOG_FMT "[%s:%06u %n:%i:%F:%l] "
#endif

struct fuse_context {
	uid_t uid;
	gid_t gid;
	pid_t pid;
};

const char			*psc_logfmt = PSC_LOG_FMT;
__static int			 psc_loglevel = PLL_TRACE;
__static struct psclog_data	*psc_logdata;
char				 psclog_eol[8] = "\n";

void
psc_log_init(void)
{
	char *ep, *p;
	long l;

	if ((p = getenv("PSC_LOG_FORMAT")) != NULL)
		psc_logfmt = p;
	if ((p = getenv("PSC_LOG_LEVEL")) != NULL) {
		ep = NULL;
		errno = 0;
		l = strtol(p, &ep, 10);
		if (p[0] == '\0' || ep == NULL || *ep != '\0')
			errx(1, "invalid log level env: %s", p);
		if (errno == ERANGE || l < 0 || l >= PNLOGLEVELS)
			errx(1, "invalid log level env: %s", p);
		psc_loglevel = (int)l;
	}
}

__weak struct psclog_data *
psclog_getdatamem(void)
{
	return (psc_logdata);
}

__weak void
psclog_setdatamem(struct psclog_data *d)
{
	psc_logdata = d;
}

int
psc_log_getlevel_global(void)
{
	return (psc_loglevel);
}

__weak int
psc_log_getlevel_ss(__unusedx int subsys)
{
	return (psc_log_getlevel_global());
}

__weak int
psc_log_getlevel(int subsys)
{
	return (psc_log_getlevel_ss(subsys));
}

void
psc_log_setlevel_global(int newlevel)
{
	if (newlevel >= PNLOGLEVELS || newlevel < 0)
		errx(1, "log level out of bounds (%d)", newlevel);
	psc_loglevel = newlevel;
}

__weak void
psc_log_setlevel_ss(__unusedx int subsys, int newlevel)
{
	psc_log_setlevel_global(newlevel);
}

__weak void
psc_log_setlevel(int subsys, int newlevel)
{
	psc_log_setlevel_ss(subsys, newlevel);
}

__weak const char *
pscthr_getname(void)
{
	return (NULL);
}

/**
 * MPI_Comm_rank - dummy overrideable MPI rank retriever.
 */
__weak int
MPI_Comm_rank(__unusedx int comm, int *rank)
{
	*rank = -1;
	return (0);
}

/**
 * fuse_get_context - dummy overrideable fuse context retriever.
 */
__weak struct fuse_context *
psclog_get_fuse_context(void)
{
	return (NULL);
}

struct psclog_data *
psclog_getdata(void)
{
	struct psclog_data *d;
	char *p;

	d = psclog_getdatamem();
	if (d == NULL) {
		d = psc_alloc(sizeof(*d), PAF_NOLOG);
		if (gethostname(d->pld_hostname,
		    sizeof(d->pld_hostname)) == -1)
			err(1, "gethostname");
		if ((p = strchr(d->pld_hostname, '.')) != NULL)
			*p = '\0';
#ifdef HAVE_CNOS
		int cnos_get_rank(void);

		d->pld_rank = cnos_get_rank();
#else
		MPI_Comm_rank(1, &d->pld_rank); /* 1=MPI_COMM_WORLD */
#endif
		psclog_setdatamem(d);
	}
	return (d);
}

void
_psclogv(const char *fn, const char *func, int line, int subsys,
    int level, int options, const char *fmt, va_list ap)
{
	char prefix[LINE_MAX], emsg[LINE_MAX], umsg[LINE_MAX];
	struct fuse_context *ctx;
	struct psclog_data *d;
	struct timeval tv;
	const char *thrname;
	int save_errno;

	save_errno = errno;

	d = psclog_getdata();
	thrname = pscthr_getname();
	if (thrname == NULL) {
		if (d->pld_nothrname[0] == '\0')
			snprintf(d->pld_nothrname,
			    sizeof(d->pld_nothrname),
			    "<%ld>", syscall(SYS_gettid));
		thrname = d->pld_nothrname;
	}

	ctx = psclog_get_fuse_context();

	gettimeofday(&tv, NULL);
	FMTSTR(prefix, sizeof(prefix), psc_logfmt,
		FMTSTRCASE('F', prefix, sizeof(prefix), "s", func)
		FMTSTRCASE('f', prefix, sizeof(prefix), "s", fn)
		FMTSTRCASE('h', prefix, sizeof(prefix), "s", d->pld_hostname)
		FMTSTRCASE('i', prefix, sizeof(prefix), "ld", syscall(SYS_gettid))
		FMTSTRCASE('L', prefix, sizeof(prefix), "d", level)
		FMTSTRCASE('l', prefix, sizeof(prefix), "d", line)
		FMTSTRCASE('n', prefix, sizeof(prefix), "s", thrname)
		FMTSTRCASE('P', prefix, sizeof(prefix), "d", ctx ? (int)ctx->pid : -1)
		FMTSTRCASE('r', prefix, sizeof(prefix), "d", d->pld_rank)
		FMTSTRCASE('s', prefix, sizeof(prefix), "lu", tv.tv_sec)
		FMTSTRCASE('t', prefix, sizeof(prefix), "d", subsys)
		FMTSTRCASE('U', prefix, sizeof(prefix), "d", ctx ? (int)ctx->uid : -1)
		FMTSTRCASE('u', prefix, sizeof(prefix), "lu", tv.tv_usec)
	);

	/*
	 * Write into intermediate buffers and send it all at once
	 * to prevent threads weaving between printf() calls.
	 */
	vsnprintf(umsg, sizeof(umsg), fmt, ap);
	if (umsg[strlen(umsg) - 1] == '\n')
		umsg[strlen(umsg) - 1] = '\0';

	if (options & PLO_ERRNO)
		snprintf(emsg, sizeof(emsg), ": %s",
		    strerror(save_errno));
	else
		emsg[0] = '\0';
	fprintf(stderr, "%s%s%s%s", prefix, umsg, emsg, psclog_eol);
	errno = save_errno; /* Restore in case it is needed further. */

	if (level == PLL_FATAL) {
		abort();
		_exit(1);
	}
}

void
_psclog(const char *fn, const char *func, int line, int subsys,
    int level, int options, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psclogv(fn, func, line, subsys, level, options, fmt, ap);
	va_end(ap);
}

__dead void
_psc_fatal(const char *fn, const char *func, int line, int subsys,
    int level, int options, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psclogv(fn, func, line, subsys, level, options, fmt, ap);
	va_end(ap);

	psc_fatalx("should not reach here");
}

__dead void
_psc_fatalv(const char *fn, const char *func, int line, int subsys,
    int level, int options, const char *fmt, va_list ap)
{
	_psclogv(fn, func, line, subsys, level, options, fmt, ap);
	psc_fatalx("should not reach here");
}

/* Keep synced with LL_* constants. */
const char *psc_loglevel_names[] = {
	"fatal",
	"error",
	"warn",
	"notice",
	"info",
	"debug",
	"trace"
};

const char *
psc_loglevel_getname(int id)
{
	if (id < 0)
		return ("<unknown>");
	else if (id >= PNLOGLEVELS)
		return ("<unknown>");
	return (psc_loglevel_names[id]);
}

int
psc_loglevel_getid(const char *name)
{
	struct {
		const char	*lvl_name;
		int		 lvl_value;
	} altloglevels[] = {
		{ "none",	PLL_FATAL },
		{ "fatals",	PLL_FATAL },
		{ "errors",	PLL_ERROR },
		{ "warning",	PLL_WARN },
		{ "warnings",	PLL_WARN },
		{ "notify",	PLL_NOTICE },
		{ "all",	PLL_TRACE }
	};
	int n;

	for (n = 0; n < PNLOGLEVELS; n++)
		if (strcasecmp(name, psc_loglevel_names[n]) == 0)
			return (n);
	for (n = 0; n < nitems(altloglevels); n++)
		if (strcasecmp(name, altloglevels[n].lvl_name) == 0)
			return (altloglevels[n].lvl_value);
	return (-1);
}
