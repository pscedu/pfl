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

#ifndef _PFL_STAT_H_
#define _PFL_STAT_H_

struct stat;

#if defined(HAVE_STB_TIM) || defined(HAVE_STB_TIMESPEC)

# ifdef HAVE_STB_TIM
#  define st_pfl_atim st_atim
#  define st_pfl_ctim st_ctim
#  define st_pfl_mtim st_mtim
# else
#  define st_pfl_atim st_atimespec
#  define st_pfl_ctim st_ctimespec
#  define st_pfl_mtim st_mtimespec
# endif

# define PFL_STB_ATIME_SET(s, ns, stb)	do { (stb)->st_pfl_atim.tv_sec = (s); (stb)->st_pfl_atim.tv_nsec = (ns); } while (0)
# define PFL_STB_MTIME_SET(s, ns, stb)	do { (stb)->st_pfl_mtim.tv_sec = (s); (stb)->st_pfl_mtim.tv_nsec = (ns); } while (0)
# define PFL_STB_CTIME_SET(s, ns, stb)	do { (stb)->st_pfl_ctim.tv_sec = (s); (stb)->st_pfl_ctim.tv_nsec = (ns); } while (0)

# define PFL_STB_ATIME_GET(stb, s, ns)	do { *(s) = (stb)->st_pfl_atim.tv_sec; *(ns) = (stb)->st_pfl_atim.tv_nsec; } while (0)
# define PFL_STB_MTIME_GET(stb, s, ns)	do { *(s) = (stb)->st_pfl_mtim.tv_sec; *(ns) = (stb)->st_pfl_mtim.tv_nsec; } while (0)
# define PFL_STB_CTIME_GET(stb, s, ns)	do { *(s) = (stb)->st_pfl_ctim.tv_sec; *(ns) = (stb)->st_pfl_ctim.tv_nsec; } while (0)
#else
# define PFL_STB_ATIME_SET(s, ns, stb)	do { (stb)->st_atime = (s); } while (0)
# define PFL_STB_MTIME_SET(s, ns, stb)	do { (stb)->st_mtime = (s); } while (0)
# define PFL_STB_CTIME_SET(s, ns, stb)	do { (stb)->st_ctime = (s); } while (0)

# define PFL_STB_ATIME_GET(stb, s, ns)	do { *(s) = (stb)->st_atime; *(ns) = 0; } while (0)
# define PFL_STB_MTIME_GET(stb, s, ns)	do { *(s) = (stb)->st_mtime; *(ns) = 0; } while (0)
# define PFL_STB_CTIME_GET(stb, s, ns)	do { *(s) = (stb)->st_ctime; *(ns) = 0; } while (0)
#endif

#define DEBUG_STATBUF(level, stb, fmt, ...)					\
	psc_log((level),							\
	    "stb (%p) dev:%"PRIu64" inode:%"PRId64" mode:0%o "			\
	    "nlink:%"PRIu64" uid:%u gid:%u "					\
	    "rdev:%"PRIu64" sz:%"PRId64" blksz:%"PSCPRI_BLKSIZE_T" "		\
	    "blkcnt:%"PRId64" atime:%lu mtime:%lu ctime:%lu " fmt,		\
	    (stb), (uint64_t)(stb)->st_dev, (stb)->st_ino, (stb)->st_mode,	\
	    (uint64_t)(stb)->st_nlink, (stb)->st_uid, (stb)->st_gid,		\
	    (uint64_t)(stb)->st_rdev, (stb)->st_size, (stb)->st_blksize,	\
	    (stb)->st_blocks, (stb)->st_atime, (stb)->st_mtime,			\
	    (stb)->st_ctime, ## __VA_ARGS__)

void	pfl_dump_statbuf(int, const struct stat *);

#endif /* _PFL_STAT_H_ */
