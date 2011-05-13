/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2010-2011, Pittsburgh Supercomputing Center (PSC).
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pfl/str.h"
#include "psc_util/alloc.h" 

int
pfl_asprintf(char **p, const char *fmt, ...)
{
	va_list ap, apd;
	int sz;

	va_start(ap, fmt);
	va_copy(apd, ap);
	sz = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	*p = PSCALLOC(sz);

	vsnprintf(*p, sz, fmt, apd);
	va_end(apd);

	return (sz);
}
