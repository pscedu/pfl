# $Id$
# %PSC_START_COPYRIGHT%
# -----------------------------------------------------------------------------
# Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

CROOTDIR=		$(realpath ${ROOTDIR})
STRIPROOTDIR=		$(subst ${CROOTDIR}/,,$1)
PATH_NAMIFY=		$(subst .,_,$(subst -,_,$(subst /,_,$1)))
FILE_CFLAGS_VARNAME=	$(call PATH_NAMIFY,$(call STRIPROOTDIR,$(abspath $1)))_CFLAGS
FILE_PCPP_FLAGS_VARNAME=$(call PATH_NAMIFY,$(call STRIPROOTDIR,$(abspath $1)))_PCPP_FLAGS
FILE_CFLAGS=		${$(call PATH_NAMIFY,$(call STRIPROOTDIR,$(realpath $1)))_CFLAGS}
FILE_PCPP_FLAGS=	${$(call PATH_NAMIFY,$(call STRIPROOTDIR,$(realpath $1)))_PCPP_FLAGS}
ADD_FILE_CFLAGS=	$(shell if ! [ -f "$(abspath $1)" ]; then echo "ADD_FILE_CFLAGS: no such file: $(abspath $1)" >&2; fi )\
			$(eval $(call FILE_CFLAGS_VARNAME,$1)+=$2)

-include ${ROOTDIR}/mk/local.mk
include ${ROOTDIR}/mk/pickle.mk

_TSRCS=			$(foreach fn,$(sort ${SRCS}),$(realpath ${fn}))

_TOBJS=			$(patsubst %.c,%.o,$(filter %.c,${_TSRCS}))
_TOBJS+=		$(patsubst %.y,%.o,$(filter %.y,${_TSRCS}))
_TOBJS+=		$(patsubst %.l,%.o,$(filter %.l,${_TSRCS}))
OBJS=			$(addprefix ${OBJDIR}/,$(notdir ${_TOBJS}))
DEPS=			$(patsubst %.o,%.dep,${OBJS})

_TSUBDIRS=		$(foreach dir,${SUBDIRS},$(realpath ${dir}))

_LEXINTM=		$(patsubst %.l,%.c,$(addprefix ${OBJDIR}/,$(notdir $(filter %.l,${_TSRCS}))))
_YACCINTM=		$(patsubst %.y,%.c,$(addprefix ${OBJDIR}/,$(notdir $(filter %.y,${_TSRCS}))))
_C_SRCS=		$(filter %.c,${_TSRCS}) ${_YACCINTM} ${_LEXINTM}

OBJDIR=			${CURDIR}/obj
DEPEND_FILE=		${OBJDIR}/.depend

LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/conn.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/handlers.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/poll.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/usocklnd.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/usocklnd_cb.c

LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/debug.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/user-lock.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/user-prim.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/user-tcpip.c

LNET_LIB_SRCS+=		${LNET_BASE}/lnet/acceptor.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/api-errno.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/api-ni.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/config.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/lib-eq.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/lib-md.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/lib-me.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/lib-move.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/lib-msg.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/lo.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/peer.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/router.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/router_proc.c

LNET_PTLLND_SRCS+=	${LNET_BASE}/ulnds/ptllnd/ptllnd.c
LNET_PTLLND_SRCS+=	${LNET_BASE}/ulnds/ptllnd/ptllnd_cb.c

PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/connection.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/events.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/export.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/import.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/nb.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/niobuf.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/packgeneric.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/rpcclient.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/rsx.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/service.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/util.c

_TINCLUDES=		$(filter-out -I%,${INCLUDES}) $(patsubst %,-I%,$(foreach \
			dir,$(patsubst -I%,%,$(filter -I%,${INCLUDES})), $(realpath ${dir})))

CFLAGS+=		${DEFINES} ${_TINCLUDES}
TARGET?=		${PROG} ${LIBRARY} ${TEST}
PROG?=			${TEST}

EXTRACT_INCLUDES=	perl -ne 'print $$& while /-I\S+\s?/gc'
EXTRACT_DEFINES=	perl -ne 'print $$& while /-D\S+\s?/gc'
EXTRACT_CFLAGS=		perl -ne 'print $$& while /-[^ID]\S+\s?/gc'

# Pre-modules processing

ifneq ($(filter ${PFL_BASE}/psc_%.c,${SRCS}),)
  MODULES+=	pfl
endif

# Process modules

ifneq ($(filter fuse,${MODULES}),)
  CFLAGS+=	${FUSE_CFLAGS}
  LDFLAGS+=	${FUSE_LIBS}
  MODULES+=	fuse-hdrs
endif

ifneq ($(filter fuse-hdrs,${MODULES}),)
  DEFINES+=	-DFUSE_USE_VERSION=${FUSE_VERSION} ${FUSE_DEFINES}
  INCLUDES+=	${FUSE_INCLUDES}
endif

ifneq ($(filter zfs,${MODULES}),)
  INCLUDES+=	-I${ZFS_BASE}
  LDFLAGS+=	${ZFS_LIBS}
  MODULES+=	z
endif

ifneq ($(filter rpc,${MODULES}),)
  SRCS+=	${PSCRPC_SRCS}
  MODULES+=	lnet
endif

ifneq ($(filter lnet,${MODULES}),)
  SRCS+=	${LNET_CFS_SRCS}
  SRCS+=	${LNET_LIB_SRCS}
  MODULES+=	lnet-hdrs lnet-nid
endif

ifneq ($(filter lnet-hdrs,${MODULES}),)
  INCLUDES+=	-I${LNET_BASE}/include
  SRC_PATH+=	${LNET_BASE}
endif

ifneq ($(filter lnet-nid,${MODULES}),)
  SRCS+=	${LNET_BASE}/libcfs/nidstrings.c
endif

ifneq ($(filter pthread,${MODULES}),)
  LDFLAGS+=	${THREAD_LIBS}
  DEFINES+=	-DHAVE_LIBPTHREAD
  MODULES+=	rt
endif

ifneq ($(filter rt,${MODULES}),)
  LDFLAGS+=	${LIBRT}
endif

ifneq ($(filter curses,${MODULES}),)
  LDFLAGS+=	${LIBCURSES}
endif

ifneq ($(filter z,${MODULES}),)
  LDFLAGS+=	${LIBZ}
endif

ifneq ($(filter l,${MODULES}),)
  LDFLAGS+=	${LIBL}
endif

ifneq ($(filter pfl,${MODULES}),)
  INCLUDES+=	-I${PFL_BASE}/include
  SRC_PATH+=	${PFL_BASE}

# XXX only do this if DEBUG is set
  SRCS+=	${PFL_BASE}/psc_util/dbgutil.c
  SRCS+=	${PFL_BASE}/psc_util/printhex.c

 ifneq ($(filter pthread,${MODULES}),)
   MODULES+=	numa
 endif
endif

ifneq ($(filter mpi,${MODULES}),)
  CC=		${MPICC}
  DEFINES+=	-DMPI
endif

ifneq ($(filter qk,${MODULES}),)
  CC=		${QKCC}
  DEFINES+=	-DQK
#INCLUDES+=	-I/opt/xt-mpt/default/mpich2-64/P2/include
#LDFLAGS+=	-L/opt/xt-mpt/default/mpich2-64/P2/lib -lmpich
endif

ifneq ($(filter zcc,${MODULES}),)
  CC=		ZINCPATH=${ZEST_BASE}/intercept/include \
		ZLIBPATH=${ZEST_BASE}/client/linux-mt ${ZEST_BASE}/scripts/zcc
endif

ifneq ($(filter numa,${MODULES}),)
  DEFINES+=	${NUMA_DEFINES}
  LDFLAGS+=	${NUMA_LIBS}
endif

ifneq ($(filter gcrypt,${MODULES}),)
  CFLAGS+=	${GCRYPT_CFLAGS}
  DEFINES+=	${GCRYPT_DEFINES}
  LDFLAGS+=	${GCRYPT_LIBS}
  INCLUDES+=	${GCRYPT_INCLUDES}
endif

ifneq ($(filter aio,${MODULES}),)
  LDFLAGS+=	${LIBAIO}
endif

# Post-modules processing

ifneq ($(filter ${PFL_BASE}/psc_util/pthrutil.c,${SRCS}),)
  SRCS+=	${PFL_BASE}/psc_ds/vbitmap.c
  SRCS+=	${PFL_BASE}/psc_util/log.c
  SRCS+=	${PFL_BASE}/psc_util/thread.c
endif

ifneq ($(filter ${PFL_BASE}/psc_util/thread.c,${SRCS}),)
  SRCS+=	${PFL_BASE}/psc_ds/lockedlist.c
  SRCS+=	${PFL_BASE}/psc_util/subsys.c
  SRCS+=	${PFL_BASE}/psc_util/waitq.c
endif

ifneq ($(filter ${PFL_BASE}/psc_util/subsys.c,${SRCS}),)
  SRCS+=	${PFL_BASE}/psc_ds/dynarray.c
endif

ifneq ($(filter ${PFL_BASE}/psc_util/log.c,${SRCS}),)
  SRCS+=	${PFL_BASE}/psc_util/alloc.c
endif

ifneq ($(filter ${PFL_BASE}/psc_util/alloc.c,${SRCS}),)
  ifneq (${DEBUG},0)
    SRCS+=	${PFL_BASE}/psc_ds/hashtbl.c
  endif
endif

ifneq ($(filter ${PFL_BASE}/psc_ds/hashtbl.c,${SRCS}),)
  SRCS+=	${PFL_BASE}/psc_ds/lockedlist.c
endif

ifneq ($(filter ${PFL_BASE}/psc_ds/lockedlist.c,${SRCS}),)
  SRCS+=	${PFL_BASE}/psc_util/list.c
endif

# OBJDIR is added to .c below since lex/yacc intermediate files get generated there.
vpath %.y $(sort $(dir $(filter %.y,${_TSRCS})))
vpath %.l $(sort $(dir $(filter %.l,${_TSRCS})))
vpath %.c $(sort $(dir $(filter %.c,${_TSRCS})) ${OBJDIR})
vpath %.dep ${OBJDIR}

all: recurse-all
	@for i in ${SRCS}; do								\
		[ -n "$$i" ] || continue;						\
		if ! [ -e "$$i" ]; then							\
			echo "$$i does not exist" >&2;					\
			exit 1;								\
		fi;									\
	done
	@if ${NOTEMPTY} "${TARGET}"; then						\
		mkdir -p ${OBJDIR};							\
		${MAKE} ${TARGET};							\
	fi

.SUFFIXES:

.PRECIOUS: ${OBJDIR}/$(notdir %.dep)

# XXX this doesn't seem to work as advertised
.SILENT: ${OBJDIR}/$(notdir %.dep)

${OBJDIR}/$(notdir %.dep) : %.c
	${ECHORUN} ${MKDEP} -D ${OBJDIR} -f $@ ${DEFINES} $(				\
	    ) $$(echo $(call FILE_CFLAGS,$<) | ${EXTRACT_DEFINES}) $(			\
	    ) ${LIBC_INCLUDES} ${_TINCLUDES} $(						\
	    ) $$(echo $(call FILE_CFLAGS,$<) | ${EXTRACT_INCLUDES}) -I$(dir $<) -I. $(realpath $<)

${OBJDIR}/$(notdir %.o) : %.c
	${PCPP} $(call FILE_PCPP_FLAGS,$<) $(realpath $<) | $(				\
	) ${CC} -x c ${CFLAGS} $(call FILE_CFLAGS,$<) -I$(dir $<) -I. - -c -o $@

${OBJDIR}/$(notdir %.E) : %.c
	${CC} ${CFLAGS} $(call FILE_CFLAGS,$<) -I$(dir $<) -I. $(realpath $<) -E -o $@

${OBJDIR}/$(notdir %.c) : %.l
	echo "${LEX} ${LFLAGS} $(realpath $<) > $@"
	${LEX} ${LFLAGS} $(realpath $<) > $@

#	$(eval $$(call FILE_PCPP_FLAGS_VARNAME,$@)+=$$(call FILE_PCPP_FLAGS,$<))
#	$(eval $$(call FILE_CFLAGS_VARNAME,$@)+=$$(call FILE_CFLAGS,$<))

${OBJDIR}/$(notdir %.c) : %.y
	${ECHORUN} ${YACC} ${YFLAGS} -o $@ $(realpath $<)

ifdef HASDEPS
  ifdef PROG
    ${PROG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}
	@printf "%s" "${PROG}:" > ${DEPEND_FILE}
	@${LIBDEP} ${LDFLAGS} ${LIBDEP_ADD} >> ${DEPEND_FILE}
  endif

  ifdef LIBRARY
    ${LIBRARY}: ${OBJS}
	${AR} ${ARFLAGS} $@ ${OBJS}
  endif
else
  ifdef PROG
    .PHONY: ${PROG}
    ${PROG}:
	@${MAKE} -s ${DEPS}
	@MAKEFILES="${DEPS} ${DEPEND_FILE}" ${MAKE} HASDEPS=1 $@
  endif

  ifdef LIBRARY
    .PHONY: ${LIBRARY}
    ${LIBRARY}:
	@${MAKE} -s ${DEPS}
	@MAKEFILES="${DEPS}" ${MAKE} HASDEPS=1 $@
  endif
endif

recurse-%:
	@for i in ${_TSUBDIRS}; do							\
		echo "===> $$i $(patsubst recurse-%,%,$@)";				\
		(cd $$i && SUBDIRS= ${MAKE} $(patsubst recurse-%,%,$@)) || exit 1;	\
	done
	@if ${NOTEMPTY} "${_TSUBDIRS}"; then						\
		echo "<=== ${CURDIR}";							\
	fi

# empty but overrideable
install-hook:

# XXX use install(1)
install: recurse-install install-hook
	@if [ -n "${LIBRARY}" ]; then							\
		${ECHORUN} mkdir -p ${INSTALLDIR}/lib;					\
		${ECHORUN} cp -pf ${LIBRARY} ${INSTALLDIR}/lib;				\
	fi
	# skip programs part of test suites
	@if [ -n "${PROG}" -a x"${PROG}" -ne x"${TEST}" ]; then				\
		${ECHORUN} mkdir -p ${INSTALLDIR}/bin;					\
		${ECHORUN} cp -pf ${PROG} ${INSTALLDIR}/bin;				\
	fi
	@if ${NOTEMPTY} "${HEADERS}"; then						\
		for i in ${HEADERS}; do							\
			if [ x"$${i%/*}" = x"$$i" ]; then				\
				_dir=${INSTALLDIR}/include/$${i%/*};			\
			else								\
				_dir=${INSTALLDIR}/include;				\
			fi;								\
			${ECHORUN} mkdir -p $$_dir;					\
			${ECHORUN} cp -rfp $$i $$_dir;					\
		done;									\
	fi

clean-core:
	${RM} -rf ${OBJDIR}
	${RM} -f ${PROG} ${LIBRARY} core.[0-9]* *.core

clean: recurse-clean clean-core

distclean: recurse-distclean clean-core
	${RM} -f TAGS cscope.*out

lint: recurse-lint ${_C_SRCS}
	@if ${NOTEMPTY} "${_TSRCS}"; then						\
		${ECHORUN} ${LINT} ${_TINCLUDES} ${DEFINES} ${_C_SRCS};			\
	fi

listsrcs: recurse-listsrcs
	@if ${NOTEMPTY} "${_TSRCS}"; then						\
		echo "${_TSRCS}";							\
	fi

runtest: recurse-runtest
	@if [ -n "${TEST}" ]; then							\
		echo "./${TEST}";							\
		./${TEST} ${TESTOPTS} || exit 1;					\
	fi

maketest: recurse-maketest ${TEST}

test:
	@${MAKE} maketest && ${MAKE} runtest

hdrclean:
	${HDRCLEAN} */*.[clyh]

# empty but overrideable
regen-hook:

regen: recurse-regen regen-hook

# empty but overrideable
prereq-hook:

prereq: recurse-prereq prereq-hook

build:
	${MAKE} clean && ${MAKE} prereq && ${MAKE} regen && ${MAKE} all

copyright:
	@find . -type f \( -name '*.[chyl]' -o -name '*.[0-9]' -o -name '*.pl' -o $(	\
	    ) -name '*.sh' -o -name '*.mk' -o -name Makefile \) $(			\
	    ) -exec ${ECHORUN} ${ROOTDIR}/tools/gencopyright.sh {} \;

doc: recurse-doc
	@if ${NOTEMPTY} "${MAN}"; then							\
		${ECHORUN} ${MDPROC} $$(echo ${MAN} $(					\
		    ) $$([ -e ${PROG}.[0-9] ] && echo ${PROG}.[0-9]) $(			\
		    ) $$([ -e ${LIBRARY}.[0-9] ] && echo ${LIBRARY}.[0-9]) |		\
		    sed 's/ /\n/' | sort -u);						\
	fi

printvar-%:
	@echo ${$(patsubst printvar-%,%,$@)}

cscope cs: recurse-cs
	cscope -Rbq $(addprefix -s,${SRC_PATH})

etags: recurse-etags
	find ${SRC_PATH} -name \*.[chly] | xargs etags

printenv:
	@env | sort
