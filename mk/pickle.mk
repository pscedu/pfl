# $Id$

ifeq ($(filter $(realpath ${ROOTDIR})/compat/%,${CURDIR}),)

PICKLELOCALMK=${ROOTDIR}/mk/gen-localdefs-pickle.mk

-include ${PICKLELOCALMK}

PICKLE_NEED_VERSION=		1

ifneq (${PICKLE_NEED_VERSION},${PICKLE_HAS_VERSION})
 $(shell ${PICKLEGEN} "${ROOTDIR}" "${PICKLE_NEED_VERSION}" "${MAKE}" "${PICKLELOCALMK}" >&2)
 include ${PICKLELOCALMK}
endif

ifndef PICKLE_HAVE_POSIX_MEMALIGN
  SRCS+=						${ROOTDIR}/psc_fsutil_libs/compat/posix_memalign.c
endif

ifdef PICKLE_HAVE_PTHREAD_MUTEX_TIMEDLOCK
  DEFINES+=						-DHAVE_PTHREAD_MUTEX_TIMEDLOCK
endif

ifndef PICKLE_HAVE_CLOCK_GETTIME
  SRCS+=						${ROOTDIR}/psc_fsutil_libs/compat/clock_gettime.c
endif

ifndef PICKLE_HAVE_HOST_NAME_MAX
  DEFINES+=						-DHOST_NAME_MAX=MAXHOSTNAMELEN
endif

endif
