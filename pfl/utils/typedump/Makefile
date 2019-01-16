# $Id$

ROOTDIR=../../..
include ${ROOTDIR}/Makefile.path

PROG=		typedump
SRCS+=		typedump.c
INCLUDES+=	-I${OBJDIR}

# pthread is specified here to use correct struct sizes
MODULES+=	lnet-hdrs pthread pfl

include ${PFLMK}

$(call ADD_FILE_PCPP_FLAGS,typedump.c,-TR)

typedump.c: ${OBJDIR}/typedump.h

${OBJDIR}/typedump.h: ${OBJDIR} ${PFL_BASE}/*.h
	${GENTYPES} -h '../../*.h' $(				\
	    ) -x '../../{fsmod,refmgr,queue,tree}.h' > $@
