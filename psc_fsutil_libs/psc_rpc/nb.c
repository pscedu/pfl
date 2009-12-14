/* $Id$ */

#define PSC_SUBSYS PSS_RPC

#include <inttypes.h>

#include "psc_util/alloc.h"
#include "psc_rpc/rpc.h"
#include "psc_rpc/rpclog.h"

/**
 * nbreqset_push - send out new requests
 *
 */
static int
nbreqset_push(struct pscrpc_request *req)
{
	return (pscrpc_push_req(req));
}

/**
 * nbreqset_init - make a non-blocking set
 * @nb_interpret:  this must only take into account completed rpcs
 * @nb_callback:   application callback
 */
struct pscrpc_nbreqset *
nbreqset_init(set_interpreter_func nb_interpret,
	      nbreq_callback       nb_callback)
{

	struct pscrpc_nbreqset *nbs;

	nbs = PSCALLOC(sizeof(struct pscrpc_nbreqset));
	pscrpc_set_init(&nbs->nb_reqset);
	nbs->nb_reqset.set_interpret = nb_interpret;
	nbs->nb_callback = nb_callback;
	atomic_set(&nbs->nb_outstanding, 0);
	return nbs;
}

/**
 * nbreqset_add - add a new non-blocking request to the mix
 *
 */
void
nbreqset_add(struct pscrpc_nbreqset *nbs,
	     struct pscrpc_request  *req)
{

	atomic_inc(&nbs->nb_outstanding);
	pscrpc_set_add_new_req(&nbs->nb_reqset, req);
	if (nbreqset_push(req)) {
		DEBUG_REQ(PLL_ERROR, req, "Send Failure");
		psc_fatalx("Send Failure");
	}
}

/**
 * nbrequest_flush - sync all outstanding requests
 */
int
nbrequest_flush(struct pscrpc_nbreqset *nbs)
{
	return (pscrpc_set_wait(&nbs->nb_reqset));
}

/**
 * nbrequest_reap - remove completed requests from the
 *                  request set and place them into the
 *                  'completed' list.
 * @nbs: the non-blocking set
 * Notes:  Call before pscrpc_check_set(), pscrpc_check_set() manages
 *        'set_remaining'..  If there are problems look at pscrpc_set_wait(),
 *        there you'll find a more sophisticated and proper use of sets.
 *        pscrpc_set_wait() deals with blocking set nevertheless much applies
 *        here as well.
 */
int
nbrequest_reap(struct pscrpc_nbreqset *nbs)
{
	int    nreaped=0, nchecked=0;
	struct psclist_head          *i, *j;
	struct pscrpc_request     *req;
	struct pscrpc_request_set *set = &nbs->nb_reqset;
	struct l_wait_info lwi;
	int timeout = 1;

	ENTRY;

	lwi = LWI_TIMEOUT(timeout, NULL, NULL);
	psc_cli_wait_event(&set->set_waitq,
		       (nreaped=pscrpc_check_set(set, 0)), &lwi);

	if (!nreaped)
		RETURN(0);

	pscrpc_set_lock(set);

	psclist_for_each_safe(i, j, &set->set_requests) {
		nchecked++;
		req = psclist_entry(i, struct pscrpc_request,
				    rq_set_chain_lentry);
		DEBUG_REQ(PLL_INFO, req, "reap if Completed");
		/*
		 * Move sent rpcs to the set_requests list
		 */
		if (req->rq_phase == PSCRQ_PHASE_COMPLETE) {
			psclist_del(&req->rq_set_chain_lentry);
			atomic_dec(&nbs->nb_outstanding);
			nreaped++;
			/*
			 * paranoia
			 */
			psc_assert(atomic_read(&nbs->nb_outstanding) >= 0);
			/*
			 * This is the caller's last shot at accessing
			 *  this msg..
			 * Let the callback deal with it's own
			 *  error handling, we can't do much from here
			 */
			if (nbs->nb_callback != NULL)
				(int)nbs->nb_callback(req,
						      &req->rq_async_args);
			/*
			 * Be done with it..
			 */
			pscrpc_req_finished(req);
		}
	}
	freelock(&set->set_lock);
	psc_dbg("checked %d requests", nchecked);
	RETURN(nreaped);
}
