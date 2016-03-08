/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2011-2016, Pittsburgh Supercomputing Center
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#include <sys/param.h>
#include <sys/stat.h>

#ifdef HAVE_STATFS_FSTYPE
# include <sys/mount.h>
#else
# include <mntent.h>
#endif

#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/log.h"
#include "pfl/net.h"
#include "pfl/str.h"
#include "pfl/sys.h"

int
pflsys_getusergroups(uid_t uid, gid_t defgid, gid_t *gv, int *ng)
{
	struct passwd pw, *pwp;
	char buf[LINE_MAX];
	int rc;

	*ng = 0;
	rc = getpwuid_r(uid, &pw, buf, sizeof(buf), &pwp);
	if (rc)
		return (rc);
	*ng = NGROUPS_MAX;
	getgrouplist(pw.pw_name, defgid, gv, ng);
	return (0);
}

int
pflsys_userisgroupmember(uid_t uid, gid_t defgid, gid_t gid)
{
	gid_t gv[NGROUPS_MAX];
	int j, ng;

	pflsys_getusergroups(uid, defgid, gv, &ng);
	for (j = 0; j < ng; j++)
		if (gid == gv[j])
			return (1);
	return (0);
}

int
pfl_systemf(const char *fmt, ...)
{
	va_list ap;
	char *buf;
	int rc;

	va_start(ap, fmt);
	pfl_vasprintf(&buf, fmt, ap);
	va_end(ap);

	rc = system(buf);

	free(buf);
	return (rc);
}

int
pfl_getfstype(const char *ofn, char *buf, size_t len)
{
#ifdef HAVE_STATFS_FSTYPE
	int rc;
	struct statfs b;

	rc = statfs(ofn, &b);
	if (rc == -1)
		return (errno);
	strlcpy(buf, b.f_fstypename, len);
#else
	char mbuf[BUFSIZ], fn[PATH_MAX];
	struct mntent *m, mntbuf;
	size_t bestlen = 0;
	FILE *fp;

	if (realpath(ofn, fn) == NULL)
		return (errno);

	fp = setmntent(_PATH_MOUNTED, "r");
	if (fp == NULL)
		return (errno);
	while ((m = getmntent_r(fp, &mntbuf, mbuf, sizeof(mbuf)))) {
		len = pfl_string_eqlen(m->mnt_dir, fn);
		if (len > bestlen) {
			bestlen = len;
			strlcpy(buf, m->mnt_type, len);
		}
	}
	endmntent(fp);
	/* XXX check for errors */
#endif
	return (0);
}

#ifndef HAVE_LIBPTHREAD

char psc_hostname[PFL_HOSTNAME_MAX];

const char *
pflsys_get_hostname(void)
{
	if (psc_hostname[0] == '\0')
		if (gethostname(psc_hostname, sizeof(psc_hostname)) == -1)
			psc_fatal("gethostname");
	return (PCPP_STR(psc_hostname));
}

#endif
