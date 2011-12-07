/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2008-2011, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#undef _FILE_OFFSET_BITS	/* FTS is not 64-bit ready */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>

#include "pfl/str.h"
#include "pfl/walk.h"
#include "psc_util/log.h"

int
pfl_filewalk(const char *fn, int flags,
    int (*cbf)(const char *, const struct stat *, void *), void *arg)
{
	char * const pathv[] = { (char *)fn, NULL };
	char buf[PATH_MAX];
	struct stat stb;
	int rc = 0;
	FTSENT *f;
	FTS *fp;

	if (flags & PFL_FILEWALKF_RECURSIVE) {
		/* XXX security implications of FTS_NOCHDIR? */
		fp = fts_open(pathv, FTS_COMFOLLOW | FTS_NOCHDIR |
		    FTS_PHYSICAL, NULL);
		if (fp == NULL)
			psc_fatal("fts_open %s", fn);
		while ((f = fts_read(fp)) != NULL) {
			switch (f->fts_info) {
			case FTS_NS:
				warnx("%s: %s", f->fts_path,
				    strerror(f->fts_errno));
				break;
			case FTS_F:
			case FTS_D:
				if (realpath(f->fts_path, buf) == NULL)
					warn("%s", f->fts_path);
				else {
					if (flags & PFL_FILEWALKF_VERBOSE)
						warnx("processing %s%s",
						    buf, f->fts_info ==
						    FTS_D ? "/" : "");
					rc = cbf(buf, f->fts_statp, arg);
					if (rc) {
						fts_close(fp);
						return (rc);
					}
				}
				break;
			case FTS_SL:
				if (flags & PFL_FILEWALKF_VERBOSE)
					warnx("processing %s%s", buf);
				rc = cbf(f->fts_path, f->fts_statp, arg);
				if (rc) {
					fts_close(fp);
					return (rc);
				}
				break;
			case FTS_DP:
				break;
			default:
				warnx("%s: %s", f->fts_path,
				    strerror(f->fts_errno));
				break;
			}
		}
		fts_close(fp);
	} else {
		if (lstat(fn, &stb) == -1)
			err(1, "%s", fn);
		else if (!S_ISREG(stb.st_mode) && !S_ISDIR(stb.st_mode))
			errx(1, "%s: not a file or directory", fn);
		else if (realpath(fn, buf) == NULL)
			err(1, "%s", fn);
		else
			rc = cbf(buf, &stb, arg);
	}
	return (rc);
}
