/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2006-2018, Pittsburgh Supercomputing Center
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

#ifndef _PFL_PFL_H_
#define _PFL_PFL_H_

#include "pfl/compat.h"

#if HAVE_PTHREAD_YIELD
#  define pscthr_yield()	pthread_yield()
#else
#  define pscthr_yield()	sched_yield()
#endif

#define PFL_PRFLAG(fl, val, seq)					\
	do {								\
		if (*(val) & (fl)) {					\
			pfl_print_flag(#fl, (seq));			\
			*(val) &= ~(fl);				\
		}							\
	} while (0)

struct pfl_callerinfo {
	const char	*pci_filename;
	const char	*pci_func;
	int		 pci_lineno;
	int		 pci_subsys;
};

#define PFL_CALLERINFOSS(ss)						\
	(_pfl_callerinfo ? _pfl_callerinfo :				\
	    (const struct pfl_callerinfo[]){{				\
		.pci_func = __func__,					\
		.pci_lineno = __LINE__,					\
		.pci_filename = __FILE__,				\
		.pci_subsys = (ss),					\
	    }})
#define PFL_CALLERINFO()		PFL_CALLERINFOSS(PSC_SUBSYS)

void	 pfl_abort(void);
void	 pfl_atexit(void (*)(void));
void	 pfl_dump_fflags(int);
void	 pfl_dump_stack(void);
pid_t	 pfl_getsysthrid(void);
void	 pfl_init(void);
void	 pfl_print_flag(const char *, int *);
void	 pfl_setprocesstitle(char **, const char *, ...);

extern pid_t	  pfl_pid;

#define PFL_CALLERINFO_ARG \
  _Pragma("GCC diagnostic push") \
  _Pragma("GCC diagnostic ignored \"-Wshadow\"") \
  const struct pfl_callerinfo *_pfl_callerinfo \
  _Pragma("GCC diagnostic pop") \

extern const struct pfl_callerinfo *_pfl_callerinfo;

#endif /* _PFL_PFL_H_ */
