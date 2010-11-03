# $Id$
# %PSC_START_COPYRIGHT%
# -----------------------------------------------------------------------------
# Copyright (c) 2010, Pittsburgh Supercomputing Center (PSC).
#
# Permission to use, copy, and modify this software and its documentation
# without fee for personal use or non-commercial use within your organization
# is hereby granted, provided that the above copyright notice is preserved in
# all copies and that the copyright and this permission notice appear in
# supporting documentation.  Permission to redistribute this software to other
# organizations or individuals is not permitted without the written permission
# of the Pittsburgh Supercomputing Center.  PSC makes no representations about
# the suitability of this software for any purpose.  It is provided "as is"
# without express or implied warranty.
# -----------------------------------------------------------------------------
# %PSC_END_COPYRIGHT%

ifeq ($(filter $(realpath ${ROOTDIR})/compat/%,${CURDIR}),)

PICKLELOCALMK=${ROOTDIR}/mk/gen-localdefs-$(word 1,$(subst ., ,$(shell hostname)))-pickle.mk

-include ${PICKLELOCALMK}

PICKLE_NEED_VERSION=					$(word 2,$$Rev$$)

 ifneq (${PICKLE_NEED_VERSION},${PICKLE_HAS_VERSION})
  $(shell ${PICKLEGEN} "${ROOTDIR}" "${PICKLE_NEED_VERSION}" "${MAKE}" "${PICKLELOCALMK}" >&2)
  include ${PICKLELOCALMK}
 endif

 ifndef PICKLE_HAVE_POSIX_MEMALIGN
  SRCS+=						${PFL_BASE}/compat/posix_memalign.c
 endif

 ifdef PICKLE_HAVE_PTHREAD_MUTEX_TIMEDLOCK
  DEFINES+=						-DHAVE_PTHREAD_MUTEX_TIMEDLOCK
 endif

 ifdef PICKLE_HAVE_CLOCK_GETTIME
  DEFINES+=						-DHAVE_CLOCK_GETTIME
 else
  SRCS+=						${PFL_BASE}/compat/clock_gettime.c
 endif

 ifdef PICKLE_HAVE_PTHREAD_BARRIER
  DEFINES+=						-DHAVE_PTHREAD_BARRIER
 else
  ifneq ($(filter pthread,${MODULES}),)
   SRCS+=						${PFL_BASE}/compat/pthread_barrier.c
   SRCS+=						${PFL_BASE}/psc_util/pthrutil.c
  endif
 endif

 ifdef PICKLE_HAVE_STRLCPY
  DEFINES+=						-DHAVE_STRLCPY
 else
  SRCS+=						${PFL_BASE}/compat/strlcpy.c
 endif

 ifdef PICKLE_HAVE_STRNLEN
  DEFINES+=						-DHAVE_STRNLEN
 else
  SRCS+=						${PFL_BASE}/compat/strnlen.c
 endif

 ifdef PICKLE_HAVE_SYS_SIGABBREV
  DEFINES+=						-DHAVE_SYS_SIGABBREV
 endif

 ifdef PICKLE_HAVE_GETHOSTBYNAME
  DEFINES+=						-DHAVE_GETHOSTBYNAME
 endif

 ifdef PICKLE_HAVE_GETIFADDRS
  DEFINES+=						-DHAVE_GETIFADDRS
 endif

 ifdef PICKLE_HAVE_GETPEEREID
  DEFINES+=						-DHAVE_GETPEEREID
 endif

 ifdef PICKLE_HAVE_RTNETLINK
  DEFINES+=						-DHAVE_RTNETLINK
 endif

 ifdef PICKLE_HAVE_I386_CMPXCHG8B
  DEFINES+=						-DHAVE_I386_CMPXCHG8B
 endif

 ifdef PICKLE_HAVE_NET_IF_DL_H
  DEFINES+=						-DHAVE_NET_IF_DL_H
 endif

 ifdef PICKLE_HAVE_RTM_HDRLEN
  DEFINES+=						-DHAVE_RTM_HDRLEN
 endif

 ifdef PICKLE_HAVE_MACH_MACH_TYPES_H
  DEFINES+=						-DHAVE_MACH_MACH_TYPES_H
 endif

 ifdef PICKLE_HAVE_LIBKERN_OSBYTEORDER_H
  DEFINES+=						-DHAVE_LIBKERN_OSBYTEORDER_H
 endif

 ifdef PICKLE_HAVE_INOTIFY
  DEFINES+=						-DHAVE_INOTIFY
 endif

 ifdef PICKLE_HAVE_ATSYSCALLS
  DEFINES+=						-DHAVE_ATSYSCALLS
 endif

 ifdef PICKLE_HAVE_STB_TIM
  DEFINES+=						-DHAVE_STB_TIM
 endif

 ifdef PICKLE_HAVE_STB_TIMESPEC
  DEFINES+=						-DHAVE_STB_TIMESPEC
 endif

 ifdef PICKLE_HAVE_SETRESUID
  DEFINES+=						-DHAVE_SETRESUID
 else
  SETRESUID_SRC=					${PFL_BASE}/compat/setresuid.c
 endif

 ifdef PICKLE_HAVE_SYNC_FILE_RANGE
  DEFINES+=						-DHAVE_SYNC_FILE_RANGE
 endif

 ifdef PICKLE_HAVE_FUSE_DEBUGLEVEL
  DEFINES+=						-DHAVE_FUSE_DEBUGLEVEL
 endif

 ifdef PICKLE_HAVE_FUSE
  DEFINES+=						-DHAVE_FUSE
  PSCFS_SRCS+=						${PFL_BASE}/psc_fs/fuse.c
 endif

 ifdef PICKLE_HAVE_NNPFS
  DEFINES+=						-DHAVE_NNPFS
  PSCFS_SRCS+=						${PFL_BASE}/psc_fs/nnpfs.c
 endif

 ifdef PICKLE_HAVE_DOKAN
  DEFINES+=						-DHAVE_DOKAN
  PSCFS_SRCS+=						${PFL_BASE}/psc_fs/dokan.c
 endif

 ifdef PICKLE_HAVE_SETPROCTITLE
  DEFINES+=						-DHAVE_SETPROCTITLE
 endif

 ifdef PICKLE_HAVE_SGIO
  DEFINES+=						-DHAVE_SGIO
 endif

 ifdef PICKLE_HAVE_FUTIMENS
  DEFINES+=						-DHAVE_FUTIMENS
  SRCS+=						${PFL_BASE}/compat/futimens.c
 endif

endif
