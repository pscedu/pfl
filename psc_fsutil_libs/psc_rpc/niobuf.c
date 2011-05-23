/* $Id$ */

/*
 *  Copyright (c) 2002, 2003 Cluster File Systems, Inc.
 *
 *   This file is part of the Lustre file system, http://www.lustre.org
 *   Lustre is a trademark of Cluster File Systems, Inc.
 *
 *   You may have signed or agreed to another license before downloading
 *   this software.  If so, you are bound by the terms and conditions
 *   of that agreement, and the following does not apply to you.  See the
 *   LICENSE file included with this distribution for more information.
 *
 *   If you did not agree to a different license, then this copy of Lustre
 *   is open source software; you can redistribute it and/or modify it
 *   under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   In either case, Lustre is distributed in the hope that it will be
 *   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   license text for more details.
 *
 */

#define PSC_SUBSYS PSS_RPC

#include <inttypes.h>

#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/log.h"
#include "psc_util/waitq.h"
#include "psc_rpc/export.h"
#include "psc_rpc/rpc.h"
#include "psc_rpc/rpclog.h"

/**
 * pscrpc_send_buf - Rudimentary send function which uses LNetPut().
 *	This is called by pscrpc_send_reply() & pscrpc_send_rpc() which
 *	use pscrpc_send_buf() to PUT RPC replies and requests.
 * @mdh:  md handle to peer.
 */
static int
pscrpc_send_buf(lnet_handle_md_t *mdh, void *base, int len,
    lnet_ack_req_t ack, struct pscrpc_cb_id *cbid,
    struct pscrpc_connection *conn, int portal, uint64_t xid)
{
	lnet_md_t md;
	int rc;

	psc_assert(portal);
	psc_assert(conn);
	CDEBUG(D_INFO, "conn=%p id %s\n", conn, libcfs_id2str(conn->c_peer));
	md.start     = base;
	md.length    = len;
	md.threshold = (ack == LNET_ACK_REQ) ? 2 : 1;
	md.options   = PSCRPC_MD_OPTIONS;
	md.user_ptr  = cbid;
	md.eq_handle = pscrpc_eq_h;

#if 0
	if (ack == LNET_ACK_REQ &&
	    OBD_FAIL_CHECK(OBD_FAIL_PSCRPC_ACK | OBD_FAIL_ONCE)) {
		/* don't ask for the ack to simulate failing client */
		ack = LNET_NOACK_REQ;
		obd_fail_loc |= OBD_FAIL_ONCE | OBD_FAILED;
	}
#endif

	rc = LNetMDBind(md, LNET_UNLINK, mdh);
	if (rc) {
		psclog_errorx("LNetMDBind failed: %d", rc);
		psc_assert(rc == -ENOMEM);
		return (-ENOMEM);
	}

	psclog_dbg("Sending %d bytes to portal %d, xid %#"PRIx64,
	    len, portal, xid);

	rc = LNetPut(conn->c_self, *mdh, ack, conn->c_peer, portal, xid,
	    0, 0);
	if (rc) {
		int rc2;

		/* We're going to get an UNLINK event when I unlink below,
		 * which will complete just like any other failed send, so
		 * I fall through and return success here! */
		psclog_errorx("LNetPut(%s, %d, %"PRIu64") failed: %d",
		    libcfs_id2str(conn->c_peer), portal, xid, rc);
		rc2 = LNetMDUnlink(*mdh);
		psc_assert(rc2 == 0);
	} else
		psc_iostats_intv_add(&conn->c_iostats_snd, len);

	return (0);
}

/**
 * pscrpc_start_bulk_transfer - Server initiated bulk data transfer.
 * @desc: the bulk data desc
 */
int
pscrpc_start_bulk_transfer(struct pscrpc_bulk_desc *desc)
{
	/* the lustre way: */
	//struct pscrpc_connection *conn = desc->bd_export->exp_connection;
	struct pscrpc_connection *conn = desc->bd_connection;
	int                        rc;
	int                        rc2;
	lnet_md_t                  md;
	uint64_t                   xid;

	//if (OBD_FAIL_CHECK_ONCE(OBD_FAIL_PSCRPC_BULK_PUT_NET))
	//        return (0);

	/* NB no locking required until desc is on the network */
	psc_assert(!desc->bd_network_rw);
	psc_assert(desc->bd_type == BULK_PUT_SOURCE ||
		 desc->bd_type == BULK_GET_SINK);
	psc_assert(conn);

	desc->bd_success = 0;

	md.max_size = 0;
	md.user_ptr = &desc->bd_cbid;
	md.eq_handle = pscrpc_eq_h;
	md.threshold = 2; /* SENT and ACK/REPLY */
	md.options = PSCRPC_MD_OPTIONS;
	pscrpc_fill_bulk_md(&md, desc);

	psc_assert(desc->bd_cbid.cbid_fn == pscrpc_server_bulk_callback);
	psc_assert(desc->bd_cbid.cbid_arg == desc);

	/* NB total length may be 0 for a read past EOF, so we send a 0
	 * length bulk, since the client expects a bulk event. */

	rc = LNetMDBind(md, LNET_UNLINK, &desc->bd_md_h);
	if (rc) {
		CERROR("LNetMDBind failed: %d\n", rc);
		psc_assert(rc == -ENOMEM);
		return (-ENOMEM);
	}

	/* Client's bulk and reply matchbits are the same */
	xid = desc->bd_req->rq_xid;
	CDEBUG(D_NET, "Transferring %u pages %u bytes via portal %d "
	       "id %s xid %"PRIx64"\n",
	       desc->bd_iov_count, desc->bd_nob, desc->bd_portal,
	       libcfs_id2str(conn->c_peer), xid);

	/* Network is about to get at the memory */
	desc->bd_network_rw = 1;

	if (desc->bd_type == BULK_PUT_SOURCE)
		rc = LNetPut(conn->c_self, desc->bd_md_h, LNET_ACK_REQ,
			     conn->c_peer, desc->bd_portal, xid, 0, 0);
	else
		rc = LNetGet(conn->c_self, desc->bd_md_h,
			     conn->c_peer, desc->bd_portal, xid, 0);

	if (rc) {
		/* Can't send, so we unlink the MD bound above.  The UNLINK
		 * event this creates will signal completion with failure,
		 * so we return SUCCESS here! */
		CERROR("Transfer(%s, %d, %"PRIx64") failed: %d\n",
		       libcfs_id2str(conn->c_peer), desc->bd_portal, xid, rc);
		rc2 = LNetMDUnlink(desc->bd_md_h);
		psc_assert(rc2 == 0);
	}

	return (0);
}

void
pscrpc_abort_bulk(struct pscrpc_bulk_desc *desc)
{
	/* Server side bulk abort.  Idempotent.  Not thread-safe (i.e. only
	 * serialises with completion callback) */
	struct l_wait_info  lwi;
	int    rc;

	//psc_assert(!in_interrupt ());		/* might sleep */

	if (!pscrpc_bulk_active(desc))		/* completed or */
		return;				/* never started */

	/* Do not send any meaningful data over the wire for evicted clients */
#if 0
	if (desc->bd_export && desc->bd_export->exp_failed)
		ptl_rpc_wipe_bulk_pages(desc);
#endif

	/* The unlink ensures the callback happens ASAP and is the last
	 * one.  If it fails, it must be because completion just happened,
	 * but we must still l_wait_event() in this case, to give liblustre
	 * a chance to run pscrpc_server_bulk_callback()*/

	LNetMDUnlink(desc->bd_md_h);

	for (;;) {
		/* Network access will complete in finite time but the HUGE
		 * timeout lets us CWARN for visibility of sluggish NALs */
		lwi = LWI_TIMEOUT(300, NULL, NULL);
		rc = pscrpc_svr_wait_event(&desc->bd_waitq,
		    !pscrpc_bulk_active(desc), &lwi, NULL);

		if (rc == 0)
			return;

		psc_assert(rc == -ETIMEDOUT);
		CWARN("Unexpectedly long timeout: desc %p\n", desc);
	}
}

/**
 * pscrpc_register_bulk - client-side registration of bulk data buffer
 * @req: the request associated with the bulk
 */
int
pscrpc_register_bulk(struct pscrpc_request *req)
{
	struct pscrpc_bulk_desc *desc = req->rq_bulk;
	lnet_process_id_t peer;
	lnet_handle_me_t  me_h;
	lnet_md_t         md;
	int rc, rc2;

	//if (OBD_FAIL_CHECK_ONCE(OBD_FAIL_PSCRPC_BULK_GET_NET))
	//        return (0);

	/* NB no locking required until desc is on the network */
	psc_assert(desc->bd_nob > 0);
	psc_assert(!desc->bd_network_rw);
	psc_assert(desc->bd_iov_count <= (int)PSCRPC_MAX_BRW_PAGES);
	psc_assert(desc->bd_req);
	psc_assert(desc->bd_type == BULK_PUT_SINK ||
		 desc->bd_type == BULK_GET_SOURCE);
	psc_assert(desc->bd_connection);

	desc->bd_success = 0;
	peer = desc->bd_import->imp_connection->c_peer;

	md.user_ptr = &desc->bd_cbid;
	md.eq_handle = pscrpc_eq_h;
	md.threshold = 1;			/* PUT or GET */
	md.options = PSCRPC_MD_OPTIONS |
		     ((desc->bd_type == BULK_GET_SOURCE) ?
		      LNET_MD_OP_GET : LNET_MD_OP_PUT);
	pscrpc_fill_bulk_md(&md, desc);

	psc_assert(desc->bd_cbid.cbid_fn == pscrpc_client_bulk_callback);
	psc_assert(desc->bd_cbid.cbid_arg == desc);

	/* XXX Registering the same xid on retried bulk makes my head
	 * explode trying to understand how the original request's bulk
	 * might interfere with the retried request -eeb */
	if (desc->bd_registered && req->rq_xid == desc->bd_last_xid)
		psc_fatalx("registered: %d rq_xid=%"PRIx64" bd_last_xid=%"PRIx64,
		  desc->bd_registered, req->rq_xid, desc->bd_last_xid);
	desc->bd_registered = 1;
	desc->bd_last_xid = req->rq_xid;

	rc = LNetMEAttach(desc->bd_portal, peer, req->rq_xid, 0,
	    LNET_UNLINK, LNET_INS_AFTER, &me_h);
	if (rc) {
		psclog_errorx("LNetMEAttach failed: %d", rc);
		psc_assert(rc == -ENOMEM);
		return (-ENOMEM);
	}

	/* About to let the network at it... */
	desc->bd_network_rw = 1;
	rc = LNetMDAttach(me_h, md, LNET_UNLINK, &desc->bd_md_h);
	if (rc) {
		psclog_errorx("LNetMDAttach failed: %d", rc);
		psc_assert(rc == -ENOMEM);
		desc->bd_network_rw = 0;
		rc2 = LNetMEUnlink(me_h);
		psc_assert(rc2 == 0);
		return (-ENOMEM);
	}

	psclog_info("Setup bulk %s buffers: %u pages %u bytes, xid %#"PRIx64", "
	    "portal %u",
	    desc->bd_type == BULK_GET_SOURCE ? "get-source" : "put-sink",
	    desc->bd_iov_count, desc->bd_nob, req->rq_xid,
	    desc->bd_portal);
	return (0);
}

struct pscrpc_connection *
pscrpc_req_getconn(struct pscrpc_request *rq)
{
	if (rq->rq_import && rq->rq_import->imp_connection)
		return (rq->rq_import->imp_connection);
	if (rq->rq_export && rq->rq_export->exp_connection)
		return (rq->rq_export->exp_connection);
	if (rq->rq_conn)
		return (rq->rq_conn);
	return (NULL);
}

/**
 * pscrpc_unregister_bulk - Client-side deregistration of bulk data buffer.
 * @req: the request associated with the bulk
 */
void
pscrpc_unregister_bulk(struct pscrpc_request *req)
{
	/* Disconnect a bulk desc from the network.  Idempotent.  Not
	 * thread-safe (i.e. only interlocks with completion callback). */
	struct pscrpc_bulk_desc *desc = req->rq_bulk;
	struct psc_waitq        *wq;
	struct l_wait_info       lwi;
	int                      rc, l, registered=0;

	l = reqlock(&desc->bd_lock);

	psclog_info("desc->bd_registered=(%d) pscrpc_bulk_active(desc)=(%d)",
		 desc->bd_registered, pscrpc_bulk_active(desc));

	if (!desc->bd_registered && !pscrpc_bulk_active(desc)) {  /* completed or */
		ureqlock(&desc->bd_lock, l);			  /* never registered */
		return;
	}
	/* bd_req NULL until registered
	 */
	psc_assert(desc->bd_req == req);
	/* Signify that this is being unlinked.
	 */
	if (desc->bd_registered) {
		desc->bd_registered = 0;
		registered = 1;
	}

	ureqlock(&desc->bd_lock, l);

	/* the unlink ensures the callback happens ASAP and is the last
	 * one.  If it fails, it must be because completion just happened,
	 * but we must still l_wait_event() in this case to give liblustre
	 * a chance to run client_bulk_callback() */
	if (registered)
		LNetMDUnlink(desc->bd_md_h);

	psc_iostats_intv_add(&pscrpc_req_getconn(req)->
	    c_iostats_rcv, desc->bd_nob_transferred);

	if (req->rq_set)
		wq = &req->rq_set->set_waitq;
	else
		wq = &req->rq_reply_waitq;

	/* This segment should only be needed for single threaded nals.
	 */
	for (;;) {
		/* Network access will complete in finite time but the HUGE
		 * timeout lets us CWARN for visibility of sluggish NALs */
		lwi = LWI_TIMEOUT(300, NULL, NULL);
		rc = pscrpc_cli_wait_event(wq,
		    !pscrpc_bulk_active(desc), &lwi);
		if (rc == 0)
			return;

		psc_assert(rc == -ETIMEDOUT);
		DEBUG_REQ(PLL_WARN, req,
			  "Unexpectedly long timeout: desc %p", desc);
	}
}

/**
 * pscrpc_send_reply - Server-side reply function.
 * @req: the request in question
 * @may_be_difficult: not sure if we're going to use this.
 */
int
pscrpc_send_reply(struct pscrpc_request *req, int may_be_difficult)
{
	struct pscrpc_service     *svc = req->rq_rqbd->rqbd_service;
	struct pscrpc_reply_state *rs  = req->rq_reply_state;
	int                        rc;

	/* We must already have a reply buffer (only pscrpc_error() may be
	 * called without one).  We must also have a request buffer which
	 * is either the actual (swabbed) incoming request, or a saved copy
	 * if this is a req saved in target_queue_final_reply(). */
	psc_assert(req->rq_reqmsg);
	psc_assert(rs);
	psc_assert(req->rq_repmsg);
	psc_assert(may_be_difficult || !rs->rs_difficult);
	psc_assert(req->rq_repmsg == &rs->rs_msg);
	psc_assert(rs->rs_cb_id.cbid_fn == pscrpc_reply_out_callback);
	psc_assert(rs->rs_cb_id.cbid_arg == rs);
	psc_assert(req->rq_repmsg);

#if PAULS_TODO
	/*
	 * pscrpc will have to place portions of the export
	 *   functionality into the service - so here the request
	 *   should probably be linked with the pscrpc_service
	 * For now we'll just shelf this check - paul
	 */
	if (req->rq_export && req->rq_export->exp_obd &&
	    req->rq_export->exp_obd->obd_fail) {
		/* Failed obd's only send ENODEV */
		req->rq_type = PSCRPC_MSG_ERR;
		req->rq_status = -ENODEV;
		CDEBUG(D_HA, "sending ENODEV from failed obd %d\n",
		       req->rq_export->exp_obd->obd_minor);
	}
#endif

	if (req->rq_type != PSCRPC_MSG_ERR)
		req->rq_type = PSCRPC_MSG_REPLY;

	req->rq_repmsg->type   = req->rq_type;
	req->rq_repmsg->status = req->rq_status;
	req->rq_repmsg->opc    = req->rq_reqmsg->opc;
	if (req->rq_conn == NULL)
		req->rq_conn = pscrpc_get_connection(req->rq_peer,
		    req->rq_self, NULL);
	else
		pscrpc_connection_addref(req->rq_conn);

	if (req->rq_conn == NULL) {
		psclog_errorx("not replying on NULL connection"); /* bug 9635 */
		return -ENOTCONN;
	}
	atomic_inc(&svc->srv_outstanding_replies);
	pscrpc_rs_addref(rs);			/* +1 ref for the network */

	rc = pscrpc_send_buf(&rs->rs_md_h, req->rq_repmsg, req->rq_replen,
	    rs->rs_difficult ? LNET_ACK_REQ : LNET_NOACK_REQ,
	    &rs->rs_cb_id, req->rq_conn,
	    svc->srv_rep_portal, req->rq_xid);
	if (rc) {
		atomic_dec(&svc->srv_outstanding_replies);
		pscrpc_rs_decref(rs);
	}
	pscrpc_put_connection(req->rq_conn);
	return rc;
}

int
pscrpc_reply(struct pscrpc_request *req)
{
	return (pscrpc_send_reply(req, 0));
}

int
pscrpc_error(struct pscrpc_request *req)
{
	int rc;

	if (!req->rq_repmsg) {
		rc = pscrpc_pack_reply(req, 0, NULL, NULL);
		if (rc)
			return (rc);
	}

	req->rq_type = PSCRPC_MSG_ERR;

	rc = pscrpc_send_reply(req, 0);
	return (rc);
}

/**
 * pscrpc_send_rpc - Client-side push of RPC request to a server.
 * @request: the request in question
 * @reply:   not sure if we're going to use this.
 */
int
pscrpc_send_rpc(struct pscrpc_request *request, int noreply)
{
	struct pscrpc_connection *connection;
	lnet_handle_me_t  reply_me_h;
	lnet_md_t         reply_md;
	int rc, rc2;

	//OBD_FAIL_RETURN(OBD_FAIL_PSCRPC_DROP_RPC, 0);
	DEBUG_REQ(PLL_DEBUG, request, "sending rpc");

	psc_assert(request->rq_type == PSCRPC_MSG_REQUEST);

	/* If this is a re-transmit, we're required to have disengaged
	 * cleanly from the previous attempt */
	psc_assert(!request->rq_receiving_reply);

#if PAULS_TODO
	if (request->rq_import->imp_obd &&
	    request->rq_import->imp_obd->obd_fail) {
		CDEBUG(D_HA, "muting rpc for failed imp obd %s\n",
		       request->rq_import->imp_obd->obd_name);
		/* this prevents us from waiting in pscrpc_queue_wait */
		request->rq_err = 1;
		return (-ENODEV);
	}
#endif
	connection = request->rq_import->imp_connection;

	if (request->rq_bulk) {
		rc = pscrpc_register_bulk(request);
		if (rc)
			return (rc);
	}

	request->rq_reqmsg->handle = request->rq_import->imp_remote_handle;
	request->rq_reqmsg->type = PSCRPC_MSG_REQUEST;
	request->rq_reqmsg->conn_cnt = request->rq_import->imp_conn_cnt;

	if (!noreply) {
		psc_assert(request->rq_replen);
		if (request->rq_repmsg == NULL)
			PSCRPC_OBD_ALLOC(request->rq_repmsg,
			    request->rq_replen);

		if (request->rq_repmsg == NULL)
			GOTO(cleanup_bulk, rc = -ENOMEM);

		rc = LNetMEAttach(request->rq_reply_portal,/*XXX FIXME bug 249*/
		    connection->c_peer, request->rq_xid, 0, LNET_UNLINK,
		    LNET_INS_AFTER, &reply_me_h);
		if (rc) {
			CERROR("LNetMEAttach failed: %d\n", rc);
			psc_assert(rc == -ENOMEM);
			GOTO(cleanup_repmsg, rc = -ENOMEM);
		}
		psclog_dbg("LNetMEAttach() gave handle %"PRIx64,
		    reply_me_h.cookie);
	}

	spinlock(&request->rq_lock);
	/* If the MD attach succeeds, there _will_ be a reply_in callback */
	request->rq_receiving_reply = !noreply;
	/* Clear any flags that may be present from previous sends. */
	request->rq_replied = 0;
	request->rq_err = 0;
	request->rq_timedout = 0;
	request->rq_net_err = 0;
	request->rq_resend = 0;
	request->rq_restart = 0;
	freelock(&request->rq_lock);

	if (!noreply) {
		reply_md.start     = request->rq_repmsg;
		reply_md.length    = request->rq_replen;
		reply_md.threshold = 1;
		reply_md.options   = PSCRPC_MD_OPTIONS | LNET_MD_OP_PUT;
		reply_md.user_ptr  = &request->rq_reply_cbid;
		reply_md.eq_handle = pscrpc_eq_h;

		psclog_dbg("LNetMDAttach() try w/ handle %"PRIx64,
		      reply_me_h.cookie);

		rc = LNetMDAttach(reply_me_h, reply_md, LNET_UNLINK,
		    &request->rq_reply_md_h);
		if (rc) {
			psclog_errorx("LNetMDAttach failed: %d", rc);
			psc_assert(rc == -ENOMEM);
			spinlock(&request->rq_lock);
			/* ...but the MD attach didn't succeed... */
			request->rq_receiving_reply = 0;
			freelock(&request->rq_lock);
			GOTO(cleanup_me, rc = -ENOMEM);
		}

		psclog_dbg("Setup reply buffer: "
		    "%u bytes xid=%"PRIx64" portal=%u",
		    request->rq_replen, request->rq_xid,
		    request->rq_reply_portal);
	}

	/* add references on request and import for pscrpc_request_out_callback */
	pscrpc_request_addref(request);
	atomic_inc(&request->rq_import->imp_inflight);

#if PAULS_TODO
	OBD_FAIL_TIMEOUT(OBD_FAIL_PSCRPC_DELAY_SEND, request->rq_timeout + 5);
#endif

	request->rq_sent = CURRENT_SECONDS;
#if 0
	pscrpc_pinger_sending_on_import(request->rq_import);
#endif
	rc = pscrpc_send_buf(&request->rq_req_md_h,
	    request->rq_reqmsg, request->rq_reqlen,
	    LNET_NOACK_REQ, &request->rq_req_cbid,
	    connection,
	    request->rq_request_portal,
	    request->rq_xid);
	if (rc == 0) {
		//pscrpc_lprocfs_rpc_sent(request);
		return (rc);
	}

	psc_iostats_intv_add(&request->rq_conn->c_iostats_snd,
	    request->rq_reqlen);

	/* drop pscrpc_request_out_callback refs, we couldn't start the send */
	atomic_dec(&request->rq_import->imp_inflight);
	pscrpc_req_finished(request);

	if (noreply)
		return (rc);
	else
		GOTO(cleanup_me, rc);
 cleanup_me:
	/* MEUnlink is safe; the PUT didn't even get off the ground, and
	 * nobody apart from the PUT's target has the right nid+XID to
	 * access the reply buffer. */
	rc2 = LNetMEUnlink(reply_me_h);
	psc_assert(rc2 == 0);
	/* UNLINKED callback called synchronously */
	psc_assert(!request->rq_receiving_reply);

 cleanup_repmsg:
	PSCRPC_OBD_FREE(request->rq_repmsg, request->rq_replen);
	request->rq_repmsg = NULL;

 cleanup_bulk:
	if (request->rq_bulk)
		pscrpc_unregister_bulk(request);

	return rc;
}

/**
 * pscrpc_register_rqbd - Server-side registration of RPC request buffers.
 * @rqbd: the request buffer pointer
 */
int
pscrpc_register_rqbd(struct pscrpc_request_buffer_desc *rqbd)
{
	struct pscrpc_service   *service = rqbd->rqbd_service;
	static lnet_process_id_t  match_id = {LNET_NID_ANY, LNET_PID_ANY};
	int                      rc;
	lnet_md_t                 md;
	lnet_handle_me_t          me_h;

	CDEBUG(D_RPCTRACE, "LNetMEAttach: portal %d\n",
	    service->srv_req_portal);

	//        if (OBD_FAIL_CHECK_ONCE(OBD_FAIL_PSCRPC_RQBD))
	//        return (-ENOMEM);

	rc = LNetMEAttach(service->srv_req_portal, match_id, 0, ~0,
	    LNET_UNLINK, LNET_INS_AFTER, &me_h);
	if (rc) {
		CERROR("LNetMEAttach failed: %d\n", rc);
		return (-ENOMEM);
	}

	psc_assert(rqbd->rqbd_refcount == 0);
	rqbd->rqbd_refcount = 1;

	md.start     = rqbd->rqbd_buffer;
	md.length    = service->srv_buf_size;
	md.max_size  = service->srv_max_req_size;
	md.threshold = LNET_MD_THRESH_INF;
	md.options   = PSCRPC_MD_OPTIONS | LNET_MD_OP_PUT | LNET_MD_MAX_SIZE;
	md.user_ptr  = &rqbd->rqbd_cbid;
	md.eq_handle = pscrpc_eq_h;

	rc = LNetMDAttach(me_h, md, LNET_UNLINK, &rqbd->rqbd_md_h);
	if (rc == 0)
		return (0);

	CERROR("LNetMDAttach failed: %d; \n", rc);
	psc_assert(rc == -ENOMEM);
	rc = LNetMEUnlink(me_h);
	psc_assert(rc == 0);
	rqbd->rqbd_refcount = 0;

	return (-ENOMEM);
}

void
pscrpc_free_reply_state(struct pscrpc_reply_state *rs)
{
#if 0
	PTLRPC_RS_DEBUG_LRU_DEL(rs);
#endif

	psc_assert(atomic_read(&rs->rs_refcount) == 0);
	psc_assert(!rs->rs_on_net);
	psc_assert(!rs->rs_scheduled);

#if WEAREEVERATTHISPOINT
	psc_assert(!rs->rs_difficult || rs->rs_handled);
	psc_assert(rs->rs_export == NULL);
	psc_assert(rs->rs_nlocks == 0);
	psc_assert(psc_listhd_empty(&rs->rs_exp_list));
	psc_assert(psc_listhd_empty(&rs->rs_obd_list));

	if (unlikely(rs->rs_prealloc)) {
		struct ptlrpc_service *svc = rs->rs_service;

		spinlock(&svc->srv_lock);
		psclist_add(&rs->rs_list_entry,
			 &svc->srv_free_rs_list);
		psc_waitq_wakeall(&svc->srv_free_rs_waitq);
		freelock(&svc->srv_lock);
	}
#endif
	PSCRPC_OBD_FREE(rs, rs->rs_size);
}

static void
_pscrpc_free_req(struct pscrpc_request *request, int locked)
{
	if (request == NULL)
		return;

	psc_assert(!request->rq_receiving_reply);
	psc_assert(request->rq_rqbd == NULL);/* client-side */
	psc_assert(psclist_disjoint(&request->rq_lentry));
	psc_assert(psclist_disjoint(&request->rq_set_chain_lentry));

	/* We must take it off the imp_replay_list first.  Otherwise, we'll set
	 * request->rq_reqmsg to NULL while osc_close is dereferencing it. */
	if (request->rq_import) {
		if (!locked)
			spinlock(&request->rq_import->imp_lock);
		//psclist_del_init(&request->rq_replay_list);
		if (!locked)
			freelock(&request->rq_import->imp_lock);
	}
	//psc_assert_msg(psc_listhd_empty(&request->rq_replay_list), "req %p", request);

	if (atomic_read(&request->rq_refcount) != 0) {
		DEBUG_REQ(PLL_ERROR, request,
			  "freeing request with nonzero refcount");
		LBUG();
	}

	if (request->rq_repmsg) {
		PSCRPC_OBD_FREE(request->rq_repmsg, request->rq_replen);
		request->rq_repmsg = NULL;
	}

	if (request->rq_import) {
		pscrpc_import_put(request->rq_import);
		request->rq_import = NULL;
	}

	if (request->rq_bulk)
		pscrpc_free_bulk(request->rq_bulk);

	if (request->rq_comp)
		atomic_dec(&request->rq_comp->rqcomp_compcnt);

	psc_assert(request->rq_reply_state == NULL);

	if (request->rq_pool) {
		//_ptlrpc_free_req_to_pool(request);

	} else {
		if (request->rq_reqmsg) {
			PSCRPC_OBD_FREE(request->rq_reqmsg, request->rq_reqlen);
			request->rq_reqmsg = NULL;
		}
		PSCRPC_OBD_FREE(request, sizeof(*request));
	}
}

void
pscrpc_free_req(struct pscrpc_request *request)
{
	_pscrpc_free_req(request, 0);
}

int
_pscrpc_req_finished(struct pscrpc_request *request, int locked)
{
	if (request == NULL)
		return (1);

	if (request == LP_POISON ||
	    request->rq_reqmsg == LP_POISON) {
		CERROR("dereferencing freed request (bug 575)\n");
		LBUG();
		return (1);
	}

#if 0
	DEBUG_REQ(D_INFO, request, "refcount now %u",
		  atomic_read(&request->rq_refcount) - 1);
#endif

	if (atomic_dec_and_test(&request->rq_refcount)) {
		_pscrpc_free_req(request, locked);
		return (1);
	}

	return (0);
}

void
pscrpc_free_bulk(struct pscrpc_bulk_desc *desc)
{
	psc_assert(desc);
	psc_assert(desc->bd_iov_count != LI_POISON);	/* not freed already */
	psc_assert(!desc->bd_network_rw);		/* network hands off or */
	psc_assert(!desc->bd_registered);

	psc_assert((desc->bd_export != NULL) ^ (desc->bd_import != NULL));
	if (desc->bd_export)
		pscrpc_export_put(desc->bd_export);
	else
		pscrpc_import_put(desc->bd_import);

	PSCRPC_OBD_FREE(desc, offsetof(struct pscrpc_bulk_desc,
	    bd_iov[desc->bd_max_iov]));
}

void
pscrpc_fill_bulk_md(lnet_md_t *md, struct pscrpc_bulk_desc *desc)
{
	psc_assert(!(md->options & (LNET_MD_IOVEC | LNET_MD_KIOV | LNET_MD_PHYS)));
	if (desc->bd_iov_count == 1) {
		md->start = desc->bd_iov[0].iov_base;
		md->length = desc->bd_iov[0].iov_len;
		return;
	}
	/* Note that md->length is used differently depending on the
	 *   number of iov's given.  If > 1 iov, set LNET_MD_IOVEC
	 *   otherwise assume a single iovec (above).
	 */
	md->options |= LNET_MD_IOVEC;
	md->start = &desc->bd_iov[0];
	md->length = desc->bd_iov_count;
}
