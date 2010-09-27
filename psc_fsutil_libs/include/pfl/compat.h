/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_COMPAT_H_
#define _PFL_COMPAT_H_

#include <sys/param.h>

#include <errno.h>

#ifndef ECOMM
#  define ECOMM		EPROTO
#endif

#ifndef EBADMSG
#  ifdef EBADRPC
#    define EBADMSG	EBADRPC
#  else
#    define EBADMSG	(ELAST + 1000)
#  endif
#endif

#ifndef HOST_NAME_MAX
#  define HOST_NAME_MAX	MAXHOSTNAMELEN
#endif

#endif /* _PFL_COMPAT_H_ */
