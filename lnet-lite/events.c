/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
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

#define DEBUG_SUBSYSTEM S_RPC

#ifdef __KERNEL__
#include <linux/module.h>
#else
#include <liblustre.h>
#endif
#include <linux/obd_class.h>
#include <linux/lustre_net.h>
#include "ptlrpc_internal.h"

lnet_handle_eq_t   ptlrpc_eq_h;

/*  
 *  Client's outgoing request callback
 */
void request_out_callback(lnet_event_t *ev)
{
        struct ptlrpc_cb_id   *cbid = ev->md.user_ptr;
        struct ptlrpc_request *req = cbid->cbid_arg;
        ENTRY;

        LASSERT (ev->type == LNET_EVENT_SEND ||
                 ev->type == LNET_EVENT_UNLINK);
        LASSERT (ev->unlinked);

        DEBUG_REQ((ev->status == 0) ? D_NET : D_ERROR, req,
                  "type %d, status %d", ev->type, ev->status);

        if (ev->type == LNET_EVENT_UNLINK || ev->status != 0) {

                /* Failed send: make it seem like the reply timed out, just
                 * like failing sends in client.c does currently...  */

                spin_lock(&req->rq_lock);
                req->rq_net_err = 1;
                spin_unlock(&req->rq_lock);

                ptlrpc_wake_client_req(req);
        }

        /* these balance the references in ptl_send_rpc() */
        atomic_dec(&req->rq_import->imp_inflight);
        ptlrpc_req_finished(req);

        EXIT;
}

/*
 * Client's incoming reply callback
 */
void reply_in_callback(lnet_event_t *ev)
{
        struct ptlrpc_cb_id   *cbid = ev->md.user_ptr;
        struct ptlrpc_request *req = cbid->cbid_arg;
        ENTRY;

        LASSERT (ev->type == LNET_EVENT_PUT ||
                 ev->type == LNET_EVENT_UNLINK);
        LASSERT (ev->unlinked);
        LASSERT (ev->md.start == req->rq_repmsg);
        LASSERT (ev->offset == 0);
        LASSERT (ev->mlength <= req->rq_replen);
        
        DEBUG_REQ((ev->status == 0) ? D_NET : D_ERROR, req,
                  "type %d, status %d", ev->type, ev->status);

        spin_lock(&req->rq_lock);

        LASSERT (req->rq_receiving_reply);
        req->rq_receiving_reply = 0;

        if (ev->type == LNET_EVENT_PUT && ev->status == 0) {
                req->rq_replied = 1;
                req->rq_nob_received = ev->mlength;
        }

        /* NB don't unlock till after wakeup; req can disappear under us
         * since we don't have our own ref */
        ptlrpc_wake_client_req(req);

        spin_unlock(&req->rq_lock);
        EXIT;
}

/* 
 * Client's bulk has been written/read
 */
void client_bulk_callback (lnet_event_t *ev)
{
        struct ptlrpc_cb_id     *cbid = ev->md.user_ptr;
        struct ptlrpc_bulk_desc *desc = cbid->cbid_arg;
        ENTRY;

        LASSERT ((desc->bd_type == BULK_PUT_SINK && 
                  ev->type == LNET_EVENT_PUT) ||
                 (desc->bd_type == BULK_GET_SOURCE &&
                  ev->type == LNET_EVENT_GET) ||
                 ev->type == LNET_EVENT_UNLINK);
        LASSERT (ev->unlinked);

        CDEBUG((ev->status == 0) ? D_NET : D_ERROR,
               "event type %d, status %d, desc %p\n", 
               ev->type, ev->status, desc);

        spin_lock(&desc->bd_lock);

        LASSERT(desc->bd_network_rw);
        desc->bd_network_rw = 0;

        if (ev->type != LNET_EVENT_UNLINK && ev->status == 0) {
                desc->bd_success = 1;
                desc->bd_nob_transferred = ev->mlength;
        }

        /* NB don't unlock till after wakeup; desc can disappear under us
         * otherwise */
        ptlrpc_wake_client_req(desc->bd_req);

        spin_unlock(&desc->bd_lock);
        EXIT;
}

/* 
 * Server's incoming request callback
 */
void request_in_callback(lnet_event_t *ev)
{
        struct ptlrpc_cb_id               *cbid = ev->md.user_ptr;
        struct ptlrpc_request_buffer_desc *rqbd = cbid->cbid_arg;
        struct ptlrpc_service             *service = rqbd->rqbd_service;
        struct ptlrpc_request             *req;
        ENTRY;

        LASSERT (ev->type == LNET_EVENT_PUT ||
                 ev->type == LNET_EVENT_UNLINK);
        LASSERT ((char *)ev->md.start >= rqbd->rqbd_buffer);
        LASSERT ((char *)ev->md.start + ev->offset + ev->mlength <=
                 rqbd->rqbd_buffer + service->srv_buf_size);

        CDEBUG((ev->status == 0) ? D_NET : D_ERROR,
               "event type %d, status %d, service %s\n", 
               ev->type, ev->status, service->srv_name);

        if (ev->unlinked) {
                /* If this is the last request message to fit in the
                 * request buffer we can use the request object embedded in
                 * rqbd.  Note that if we failed to allocate a request,
                 * we'd have to re-post the rqbd, which we can't do in this
                 * context. */
                req = &rqbd->rqbd_req;
                memset(req, 0, sizeof (*req));
        } else {
                LASSERT (ev->type == LNET_EVENT_PUT);
                if (ev->status != 0) {
                        /* We moaned above already... */
                        return;
                }
                OBD_ALLOC_GFP(req, sizeof(*req), GFP_ATOMIC);
                if (req == NULL) {
                        CERROR("Can't allocate incoming request descriptor: "
                               "Dropping %s RPC from %s\n",
                               service->srv_name, 
                               libcfs_id2str(ev->initiator));
                        return;
                }
        }

        /* NB we ABSOLUTELY RELY on req being zeroed, so pointers are NULL,
         * flags are reset and scalars are zero.  We only set the message
         * size to non-zero if this was a successful receive. */
        req->rq_xid = ev->match_bits;
        req->rq_reqmsg = ev->md.start + ev->offset;
        if (ev->type == LNET_EVENT_PUT && ev->status == 0)
                req->rq_reqlen = ev->mlength;
        do_gettimeofday(&req->rq_arrival_time);
        req->rq_peer = ev->initiator;
        req->rq_self = ev->target.nid;
        req->rq_rqbd = rqbd;
        req->rq_phase = RQ_PHASE_NEW;
#ifdef CRAY_XT3
        req->rq_uid = ev->uid;
#endif

        spin_lock(&service->srv_lock);

        req->rq_history_seq = service->srv_request_seq++;
        list_add_tail(&req->rq_history_list, &service->srv_request_history);

        if (ev->unlinked) {
                service->srv_nrqbd_receiving--;

                CDEBUG(D_NET,"Buffer complete: %d buffers still posted\n",
                       service->srv_nrqbd_receiving);

                /* Normally, don't complain about 0 buffers posted; LNET won't
                 * drop incoming reqs since we set the portal lazy */
                if (test_req_buffer_pressure &&
                    ev->type != LNET_EVENT_UNLINK &&
                    service->srv_nrqbd_receiving == 0)
                        CWARN("All %s request buffers busy\n",
                              service->srv_name);

                /* req takes over the network's ref on rqbd */
        } else {
                /* req takes a ref on rqbd */
                rqbd->rqbd_refcount++;
        }

        list_add_tail(&req->rq_list, &service->srv_request_queue);
        service->srv_n_queued_reqs++;

        /* NB everything can disappear under us once the request
         * has been queued and we unlock, so do the wake now... */
        wake_up(&service->srv_waitq);

        spin_unlock(&service->srv_lock);
        EXIT;
}

/*
 *  Server's outgoing reply callback
 */
void reply_out_callback(lnet_event_t *ev)
{
        struct ptlrpc_cb_id       *cbid = ev->md.user_ptr;
        struct ptlrpc_reply_state *rs = cbid->cbid_arg;
        struct ptlrpc_service     *svc = rs->rs_service;
        ENTRY;

        LASSERT (ev->type == LNET_EVENT_SEND ||
                 ev->type == LNET_EVENT_ACK ||
                 ev->type == LNET_EVENT_UNLINK);

        if (!rs->rs_difficult) {
                /* 'Easy' replies have no further processing so I drop the
                 * net's ref on 'rs' */
                LASSERT (ev->unlinked);
                ptlrpc_rs_decref(rs);
                atomic_dec (&svc->srv_outstanding_replies);
                EXIT;
                return;
        }

        LASSERT (rs->rs_on_net);

        if (ev->unlinked) {
                /* Last network callback.  The net's ref on 'rs' stays put
                 * until ptlrpc_server_handle_reply() is done with it */
                spin_lock(&svc->srv_lock);
                rs->rs_on_net = 0;
                ptlrpc_schedule_difficult_reply (rs);
                spin_unlock(&svc->srv_lock);
        }

        EXIT;
}

/*
 * Server's bulk completion callback
 */
void server_bulk_callback (lnet_event_t *ev)
{
        struct ptlrpc_cb_id     *cbid = ev->md.user_ptr;
        struct ptlrpc_bulk_desc *desc = cbid->cbid_arg;
        ENTRY;

        LASSERT (ev->type == LNET_EVENT_SEND ||
                 ev->type == LNET_EVENT_UNLINK ||
                 (desc->bd_type == BULK_PUT_SOURCE &&
                  ev->type == LNET_EVENT_ACK) ||
                 (desc->bd_type == BULK_GET_SINK &&
                  ev->type == LNET_EVENT_REPLY));

        CDEBUG((ev->status == 0) ? D_NET : D_ERROR,
               "event type %d, status %d, desc %p\n", 
               ev->type, ev->status, desc);

        spin_lock(&desc->bd_lock);
        
        if ((ev->type == LNET_EVENT_ACK ||
             ev->type == LNET_EVENT_REPLY) &&
            ev->status == 0) {
                /* We heard back from the peer, so even if we get this
                 * before the SENT event (oh yes we can), we know we
                 * read/wrote the peer buffer and how much... */
                desc->bd_success = 1;
                desc->bd_nob_transferred = ev->mlength;
        }

        if (ev->unlinked) {
                /* This is the last callback no matter what... */
                desc->bd_network_rw = 0;
                wake_up(&desc->bd_waitq);
        }

        spin_unlock(&desc->bd_lock);
        EXIT;
}

static void ptlrpc_master_callback(lnet_event_t *ev)
{
        struct ptlrpc_cb_id *cbid = ev->md.user_ptr;
        void (*callback)(lnet_event_t *ev) = cbid->cbid_fn;

        /* Honestly, it's best to find out early. */
        LASSERT (cbid->cbid_arg != LP_POISON);
        LASSERT (callback == request_out_callback ||
                 callback == reply_in_callback ||
                 callback == client_bulk_callback ||
                 callback == request_in_callback ||
                 callback == reply_out_callback ||
                 callback == server_bulk_callback);
        
        callback (ev);
}

int ptlrpc_uuid_to_peer (struct obd_uuid *uuid, 
                         lnet_process_id_t *peer, lnet_nid_t *self)
{
        int               best_dist = 0;
        int               best_order = 0;
        int               count = 0;
        int               rc = -ENOENT;
        int               portals_compatibility;
        int               dist;
        int               order;
        lnet_nid_t        dst_nid;
        lnet_nid_t        src_nid;

        portals_compatibility = LNetCtl(IOC_LIBCFS_PORTALS_COMPATIBILITY, NULL);

        peer->pid = LUSTRE_SRV_LNET_PID;

        /* Choose the matching UUID that's closest */
        while (lustre_uuid_to_peer(uuid->uuid, &dst_nid, count++) == 0) {
                dist = LNetDist(dst_nid, &src_nid, &order);
                if (dist < 0)
                        continue;

                if (dist == 0) {                /* local! use loopback LND */
                        peer->nid = *self = LNET_MKNID(LNET_MKNET(LOLND, 0), 0);
                        rc = 0;
                        break;
                }
                
                LASSERT (order >= 0);
                if (rc < 0 ||
                    dist < best_dist ||
                    (dist == best_dist && order < best_order)) {
                        best_dist = dist;
                        best_order = order;

                        if (portals_compatibility > 1) {
                                /* Strong portals compatibility: Zero the nid's
                                 * NET, so if I'm reading new config logs, or
                                 * getting configured by (new) lconf I can
                                 * still talk to old servers. */
                                dst_nid = LNET_MKNID(0, LNET_NIDADDR(dst_nid));
                                src_nid = LNET_MKNID(0, LNET_NIDADDR(src_nid));
                        }
                        peer->nid = dst_nid;
                        *self = src_nid;
                        rc = 0;
                }
        }

        CDEBUG(D_NET,"%s->%s\n", uuid->uuid, libcfs_id2str(*peer));
        if (rc != 0) 
                CERROR("No NID found for %s\n", uuid->uuid);
        return rc;
}

void ptlrpc_ni_fini(void)
{
        wait_queue_head_t   waitq;
        struct l_wait_info  lwi;
        int                 rc;
        int                 retries;
        
        /* Wait for the event queue to become idle since there may still be
         * messages in flight with pending events (i.e. the fire-and-forget
         * messages == client requests and "non-difficult" server
         * replies */

        for (retries = 0;; retries++) {
                rc = LNetEQFree(ptlrpc_eq_h);
                switch (rc) {
                default:
                        LBUG();

                case 0:
                        LNetNIFini();
                        return;
                        
                case -EBUSY:
                        if (retries != 0)
                                CWARN("Event queue still busy\n");
                        
                        /* Wait for a bit */
                        init_waitqueue_head(&waitq);
                        lwi = LWI_TIMEOUT(2, NULL, NULL);
                        l_wait_event(waitq, 0, &lwi);
                        break;
                }
        }
        /* notreached */
}

lnet_pid_t ptl_get_pid(void)
{
        lnet_pid_t        pid;

#ifndef  __KERNEL__
        pid = getpid();
#else
        pid = LUSTRE_SRV_LNET_PID;
#endif
        return pid;
}
        
int ptlrpc_ni_init(void)
{
        int              rc;
        lnet_pid_t       pid;

        pid = ptl_get_pid();
        CDEBUG(D_NET, "My pid is: %x\n", pid);

        /* We're not passing any limits yet... */
        rc = LNetNIInit(pid);
        if (rc < 0) {
                CDEBUG (D_NET, "Can't init network interface: %d\n", rc);
                return (-ENOENT);
        }

        /* CAVEAT EMPTOR: how we process portals events is _radically_
         * different depending on... */
#ifdef __KERNEL__
        /* kernel portals calls our master callback when events are added to
         * the event queue.  In fact lustre never pulls events off this queue,
         * so it's only sized for some debug history. */
        rc = LNetEQAlloc(1024, ptlrpc_master_callback, &ptlrpc_eq_h);
#else
        /* liblustre calls the master callback when it removes events from the
         * event queue.  The event queue has to be big enough not to drop
         * anything */
        rc = LNetEQAlloc(10240, LNET_EQ_HANDLER_NONE, &ptlrpc_eq_h);
#endif
        if (rc == 0)
                return 0;

        CERROR ("Failed to allocate event queue: %d\n", rc);
        LNetNIFini();

        return (-ENOMEM);
}

#ifndef __KERNEL__
LIST_HEAD(liblustre_wait_callbacks);
void *liblustre_services_callback;

void *
liblustre_register_wait_callback (int (*fn)(void *arg), void *arg)
{
        struct liblustre_wait_callback *llwc;
        
        OBD_ALLOC(llwc, sizeof(*llwc));
        LASSERT (llwc != NULL);
        
        llwc->llwc_fn = fn;
        llwc->llwc_arg = arg;
        list_add_tail(&llwc->llwc_list, &liblustre_wait_callbacks);
        
        return (llwc);
}

void
liblustre_deregister_wait_callback (void *opaque)
{
        struct liblustre_wait_callback *llwc = opaque;
        
        list_del(&llwc->llwc_list);
        OBD_FREE(llwc, sizeof(*llwc));
}

int
liblustre_check_events (int timeout)
{
        lnet_event_t ev;
        int         rc;
        int         i;
        ENTRY;

        rc = LNetEQPoll(&ptlrpc_eq_h, 1, timeout * 1000, &ev, &i);
        if (rc == 0)
                RETURN(0);
        
        LASSERT (rc == -EOVERFLOW || rc == 1);
        
        /* liblustre: no asynch callback so we can't affort to miss any
         * events... */
        if (rc == -EOVERFLOW) {
                CERROR ("Dropped an event!!!\n");
                abort();
        }
        
        ptlrpc_master_callback (&ev);
        RETURN(1);
}

int liblustre_waiting = 0;

int
liblustre_wait_event (int timeout)
{
        struct list_head               *tmp;
        struct liblustre_wait_callback *llwc;
        int                             found_something = 0;

        /* single threaded recursion check... */
        liblustre_waiting = 1;

        for (;;) {
                /* Deal with all pending events */
                while (liblustre_check_events(0))
                        found_something = 1;

                /* Give all registered callbacks a bite at the cherry */
                list_for_each(tmp, &liblustre_wait_callbacks) {
                        llwc = list_entry(tmp, struct liblustre_wait_callback, 
                                          llwc_list);
                
                        if (llwc->llwc_fn(llwc->llwc_arg))
                                found_something = 1;
                }

                if (found_something || timeout == 0)
                        break;

                /* Nothing so far, but I'm allowed to block... */
                found_something = liblustre_check_events(timeout);
                if (!found_something)           /* still nothing */
                        break;                  /* I timed out */
        }

        liblustre_waiting = 0;

        return found_something;
}

#endif /* __KERNEL__ */

int ptlrpc_init_portals(void)
{
        int   rc = ptlrpc_ni_init();

        if (rc != 0) {
                CERROR("network initialisation failed\n");
                return -EIO;
        }
#ifndef __KERNEL__
        liblustre_services_callback = 
                liblustre_register_wait_callback(&liblustre_check_services, NULL);
#endif
        return 0;
}

void ptlrpc_exit_portals(void)
{
#ifndef __KERNEL__
        liblustre_deregister_wait_callback(liblustre_services_callback);
#endif
        ptlrpc_ni_fini();
}
