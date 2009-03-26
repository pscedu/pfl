/* $Id$ */

#ifndef _PSCRPC_SERVICE_H_
#define _PSCRPC_SERVICE_H_

#include "psc_rpc/rpc.h"
#include "psc_ds/list.h"

struct psc_thread;

typedef int (*pscrpc_service_func_t)(struct pscrpc_request *);

#define PSCRPC_SVCNAME_MAX 32

struct pscrpc_svc_handle {
	struct psclist_head	  svh_lentry;
	struct psc_thread	**svh_threads;
	struct pscrpc_service	 *svh_service;
	pscrpc_service_func_t	  svh_handler;
	int			  svh_nthreads;
	int			  svh_nbufs;
	int			  svh_bufsz;
	int			  svh_reqsz;
	int			  svh_repsz;
	int			  svh_req_portal;
	int			  svh_rep_portal;
	int			  svh_type;
	int			  svh_flags;
	char			  svh_svc_name[PSCRPC_SVCNAME_MAX];
};

typedef struct pscrpc_svc_handle pscrpc_svc_handle_t;

#define PSCRPC_SVCF_COUNT_PEER_QLENS	(1 << 0)

struct pscrpc_thread {
	struct pscrpc_service	*prt_svc;
};

#define pscrpc_thread_spawn(svh, type) \
	_pscrpc_thread_spawn((svh), sizeof(type))

void _pscrpc_thread_spawn(pscrpc_svc_handle_t *, size_t);

#endif /* _PSCRPC_SERVICE_H_ */
