# $Id$

ROOTDIR=.
include ${ROOTDIR}/Makefile.path

SUBDIRS+=	apps/sft
SUBDIRS+=	fio
SUBDIRS+=	psc_fsutil_libs/tests
SUBDIRS+=	psc_fsutil_libs/utils
SUBDIRS+=	slash_nara
SUBDIRS+=	slash_nara/tests
SUBDIRS+=	slash_nara/utils
SUBDIRS+=	zest
SUBDIRS+=	zest/client/linux
SUBDIRS+=	zest/tests
SUBDIRS+=	zest/utils

MAN+=		${ROOTDIR}/doc/pflenv.7

include ${MAINMK}
