# $Id$

-include ${ROOTDIR}/mk/local.mk

_TSRCS=		$(foreach fn,${SRCS},$(realpath ${fn}))

_TOBJS=		$(patsubst %.c,%.o,$(filter %.c,${_TSRCS}))
_TOBJS+=	$(patsubst %.y,%.o,$(filter %.y,${_TSRCS}))
_TOBJS+=	$(patsubst %.l,%.o,$(filter %.l,${_TSRCS}))
OBJS=		$(addprefix ${OBJDIR}/,$(notdir ${_TOBJS}))

_LEXINTM=	$(patsubst %.l,%.c,$(addprefix ${OBJDIR}/,$(notdir $(filter %.l,${_TSRCS}))))
_YACCINTM=	$(patsubst %.y,%.c,$(addprefix ${OBJDIR}/,$(notdir $(filter %.y,${_TSRCS}))))
CLEANFILES+=	$(patsubst %.y,%.h,$(notdir $(filter %.y,${_TSRCS})))
_C_SRCS=	$(filter %.c,${_TSRCS}) ${_YACCINTM} ${_LEXINTM}
ECHORUN=	echorun() { echo "$$@"; "$$@"; }; echorun

LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/conn.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/handlers.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/poll.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/usocklnd.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/usocklnd_cb.c

LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/debug.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/nidstrings.c
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
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/niobuf.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/packgeneric.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/rpcclient.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/service.c

_TINCLUDES=		$(filter-out -I%,${INCLUDES}) $(patsubst %,-I%,$(foreach \
			dir,$(patsubst -I%,%,$(filter -I%,${INCLUDES})), $(realpath ${dir})))

CFLAGS+=		${DEFINES} ${_TINCLUDES}
TARGET?=		${PROG} ${LIBRARY}
OBJDIR=			${CURDIR}/obj

# OBJDIR is added to .c below since lex/yacc intermediate files get generated there.
vpath %.y $(sort $(dir $(filter %.y,${_TSRCS})))
vpath %.l $(sort $(dir $(filter %.l,${_TSRCS})))
vpath %.c $(sort $(dir $(filter %.c,${_TSRCS})) ${OBJDIR})

all: recurse-all ${TARGET}

.SUFFIXES:

${OBJDIR}:
	mkdir -p $@

${OBJDIR}/$(notdir %.c) : %.l
	${LEX} ${LFLAGS} $(realpath $<) > $@

${OBJDIR}/$(notdir %.c) : %.y
	${YACC} ${YFLAGS} -o $@ $(realpath $<)

${OBJDIR}/$(notdir %.o) : %.c
	${CC} ${CFLAGS} ${$(subst .,_,$(subst -,_,$(subst /,_,$(subst			\
	    ../,,$(subst //,/,$(subst $(realpath					\
	    ${ROOTDIR})/,,$(realpath $<)))))))_CFLAGS} $(realpath $<) -c -o $@

${OBJDIR}/$(notdir %.E) : %.c
	${CC} ${CFLAGS} ${$(subst .,_,$(subst -,_,$(subst /,_,$(subst			\
	    ../,,$(subst //,/,$(subst $(realpath					\
	    ${ROOTDIR})/,,$(realpath $<)))))))_CFLAGS} $(realpath $<) -E -o $@

${PROG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

${LIBRARY}: ${OBJS}
	${AR} ${ARFLAGS} $@ ${OBJS}

recurse-%:
	@for i in ${SUBDIRS}; do							\
		echo -n "===> ";							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX};						\
		fi;									\
		echo $$i;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/			\
		    $(patsubst recurse-%,%,$@)) || exit 1;				\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!';			\
		fi;									\
	done

install: recurse-install
	@if [ -n "${LIBRARY}" ]; then							\
		mkdir -p ${INSTALLDIR}/lib;						\
		echo cp -pf ${LIBRARY} ${INSTALLDIR}/lib;				\
		cp -pf ${LIBRARY} ${INSTALLDIR}/lib;					\
	fi
	@if [ -n "${PROG}" ]; then							\
		mkdir -p ${INSTALLDIR}/bin;						\
		echo cp -pf ${PROG} ${INSTALLDIR}/bin;					\
		cp -pf ${PROG} ${INSTALLDIR}/bin;					\
	fi
	@if ${NOTEMPTY} "${HEADERS}"; then						\
		for i in "${HEADERS}"; do						\
			if [ x"$${i%/*}" = x"$$i" ]; then				\
				_dir=${INSTALLDIR}/include/$${i%/*};			\
			else								\
				_dir=${INSTALLDIR}/include;				\
			fi;								\
			mkdir -p $$_dir;						\
			${ECHORUN} cp -rfp $$i $$_dir;					\
		done;									\
	fi

depend: ${_C_SRCS} recurse-depend
	@if ${NOTEMPTY} "${_C_SRCS}"; then						\
		${ECHORUN} ${MKDEP} ${_TINCLUDES} ${DEFINES} ${_C_SRCS};		\
	fi
	@if [ -n "${PROG}" ]; then							\
		echo -n "${PROG}:" >> .depend;						\
		perl ${ROOTDIR}/tools/libdep.pl ${LDFLAGS} >> .depend;			\
	fi
	@# XXX add cscope here?

clean: recurse-clean
	@# Check existence of files to catch errors such as SRCS+=file.y instead of file.c
	@for i in ${_TSRCS}; do								\
		test -f $$i || { echo "file does not exist: $$i" >&2; exit 1; };	\
	done
	rm -f ${OBJS} ${PROG} ${LIBRARY} $(addprefix ${OBJDIR}/,${CLEANFILES})		\
	    ${_YACCINTM} ${_LEXINTM} .depend* TAGS cscope.out core.[0-9]*

lint: recurse-lint
	@if ${NOTEMPTY} "${_TSRCS}"; then						\
		${ECHORUN} ${LINT} ${_TINCLUDES} ${DEFINES} ${_TSRCS};			\
	fi

listsrcs: recurse-listsrcs
	@if ${NOTEMPTY} "${_TSRCS}"; then						\
		echo "${_TSRCS}";							\
	fi

test: all recurse-test
	@if [ -n "${PROG}" ]; then							\
		echo "./${PROG}";							\
		./${PROG} || exit 1;							\
	fi

hdrclean:
	${HDRCLEAN} */*.[clyh]

# empty but overrideable
prereq:

build: prereq
	${MAKE} clean && ${MAKE} depend && ${MAKE}

#CS_ARGS+=-s${APP_BASE}
#ET_ARGS+="${APP_BASE}"

ifdef SLASH_BASE
CS_ARGS+=-s${SLASH_BASE}
ET_ARGS+="${SLASH_BASE}"

CS_ARGS+=-s${ZFS_BASE}/zfs-fuse
ET_ARGS+="${ZFS_BASE}/zfs-fuse"
endif

ifdef ZEST_BASE
CS_ARGS+=-s${ZEST_BASE}
ET_ARGS+="${ZEST_BASE}"
endif

cscope cs: recurse-cs
	cscope -Rb ${CS_ARGS} -s${PFL_BASE} -s${LNET_BASE}

etags: recurse-etags
	find ${ET_ARGS} ${PFL_BASE} ${PFL_BASE} -name \*.[chly] | xargs etags

env:
	@env

-include .depend
