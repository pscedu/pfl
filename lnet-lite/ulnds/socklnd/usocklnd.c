/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2007 Cluster File Systems, Inc.
 *   Author: Maxim Patlasov <maxim@clusterfs.com>
 *
 *   This file is part of the Lustre file system, http://www.lustre.org
 *   Lustre is a trademark of Cluster File Systems, Inc.
 *
 */

#include "usocklnd.h"
#include <sys/time.h>

#if defined(__sun__) || defined(__sun)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

lnd_t the_tcplnd = {
        .lnd_type      = SOCKLND,
        .lnd_startup   = usocklnd_startup,
        .lnd_shutdown  = usocklnd_shutdown,
        .lnd_send      = usocklnd_send,
        .lnd_recv      = usocklnd_recv,
        .lnd_accept    = usocklnd_accept,
};

usock_data_t usock_data;
usock_tunables_t usock_tuns = {
        .ut_timeout         = 50,
        .ut_poll_timeout    = 1,
        .ut_fair_limit      = 1,
        .ut_npollthreads    = 0,
        .ut_min_bulk        = 1<<10,
        .ut_txcredits       = 256,
        .ut_peertxcredits   = 8,
        .ut_socknagle       = 0,
        .ut_sockbufsiz      = 0,
        .ut_usesdp          = 0,
        .ut_portinc         = 0,
};

#define MAX_REASONABLE_TIMEOUT 36000 /* 10 hours */
#define MAX_REASONABLE_NPT 1000

int
usocklnd_validate_tunables()
{
        if (usock_tuns.ut_timeout <= 0 ||
            usock_tuns.ut_timeout > MAX_REASONABLE_TIMEOUT) {
                CERROR("USOCK_TIMEOUT: %d is out of reasonable limits\n",
                       usock_tuns.ut_timeout);
                return -1;
        }

        if (usock_tuns.ut_poll_timeout <= 0 ||
            usock_tuns.ut_poll_timeout > MAX_REASONABLE_TIMEOUT) {
                CERROR("USOCK_POLL_TIMEOUT: %d is out of reasonable limits\n",
                       usock_tuns.ut_poll_timeout);
                return -1;
        }

        if (usock_tuns.ut_fair_limit <= 0) {
                CERROR("Invalid USOCK_FAIR_LIMIT: %d (should be >0)\n",
                       usock_tuns.ut_fair_limit);
                return -1;
        }

        if (usock_tuns.ut_npollthreads < 0 ||
            usock_tuns.ut_npollthreads > MAX_REASONABLE_NPT) {
                CERROR("USOCK_NPOLLTHREADS: %d is out of reasonable limits\n",
                       usock_tuns.ut_npollthreads);
                return -1;
        }

        if (usock_tuns.ut_txcredits <= 0) {
                CERROR("USOCK_TXCREDITS: %d should be positive\n",
                       usock_tuns.ut_txcredits);
                return -1;
        }

        if (usock_tuns.ut_peertxcredits <= 0) {
                CERROR("USOCK_PEERTXCREDITS: %d should be positive\n",
                       usock_tuns.ut_peertxcredits);
                return -1;
        }

        if (usock_tuns.ut_peertxcredits > usock_tuns.ut_txcredits) {
                CERROR("USOCK_PEERTXCREDITS: %d should not be greater"
                       " than USOCK_TXCREDITS: %d\n",
                       usock_tuns.ut_peertxcredits, usock_tuns.ut_txcredits);
                return -1;
        }

        if (usock_tuns.ut_socknagle != 0 &&
            usock_tuns.ut_socknagle != 1) {
                CERROR("USOCK_SOCKNAGLE: %d should be 0 or 1\n",
                       usock_tuns.ut_socknagle);
                return -1;
        }

        if (usock_tuns.ut_sockbufsiz < 0) {
                CERROR("USOCK_SOCKBUFSIZ: %d should be 0 or positive\n",
                       usock_tuns.ut_sockbufsiz);
                return -1;
        }

        return 0;
}

void
usocklnd_release_poll_states(int n)
{
        int i;

        for (i = 0; i < n; i++) {
                usock_pollthread_t *pt = &usock_data.ud_pollthreads[i];

#if defined(__sun__) || defined(__sun)
                LIBCFS_FREE (pt->upt_results,
                             sizeof(struct pollfd) * pt->upt_npollfd);
                LIBCFS_FREE (pt->upt_requests,
                             sizeof(struct pollfd) * pt->upt_npollfd);

                close(pt->upt_dp_fd);
#endif

                close(pt->upt_notifier_fd);
                close(pt->upt_pollfd[0].fd);

                pthread_mutex_destroy(&pt->upt_pollrequests_lock);
                cfs_fini_completion(&pt->upt_completion);

                LIBCFS_FREE (pt->upt_skip,
                             sizeof(int) * pt->upt_npollfd);
                LIBCFS_FREE (pt->upt_pollfd,
                             sizeof(struct pollfd) * pt->upt_npollfd);
                LIBCFS_FREE (pt->upt_idx2conn,
                              sizeof(usock_conn_t *) * pt->upt_npollfd);
                LIBCFS_FREE (pt->upt_fd2idx,
                              sizeof(int) * pt->upt_nfd2idx);
        }
}

int
usocklnd_update_tunables()
{
        int rc;

        rc = cfs_parse_int_tunable(&usock_tuns.ut_timeout,
                                      "USOCK_TIMEOUT");
        if (rc)
                return rc;

        rc = cfs_parse_int_tunable(&usock_tuns.ut_poll_timeout,
                                      "USOCK_POLL_TIMEOUT");
        if (rc)
                return rc;

        rc = cfs_parse_int_tunable(&usock_tuns.ut_npollthreads,
                                      "USOCK_NPOLLTHREADS");
        if (rc)
                return rc;

        rc = cfs_parse_int_tunable(&usock_tuns.ut_fair_limit,
                                      "USOCK_FAIR_LIMIT");
        if (rc)
                return rc;

        rc = cfs_parse_int_tunable(&usock_tuns.ut_min_bulk,
                                      "USOCK_MIN_BULK");
        if (rc)
                return rc;

        rc = cfs_parse_int_tunable(&usock_tuns.ut_txcredits,
                                      "USOCK_TXCREDITS");
        if (rc)
                return rc;

        rc = cfs_parse_int_tunable(&usock_tuns.ut_peertxcredits,
                                      "USOCK_PEERTXCREDITS");
        if (rc)
                return rc;

        rc = cfs_parse_int_tunable(&usock_tuns.ut_socknagle,
                                      "USOCK_SOCKNAGLE");
        if (rc)
                return rc;

        rc = cfs_parse_int_tunable(&usock_tuns.ut_sockbufsiz,
                                      "USOCK_SOCKBUFSIZ");
        if (rc)
                return rc;

        rc = cfs_parse_int_tunable(&usock_tuns.ut_usesdp,
                                      "USOCK_USESDP");
        if (rc)
                return rc;

        rc = cfs_parse_int_tunable(&usock_tuns.ut_portinc,
                                      "USOCK_PORTINC");
        if (rc)
                return rc;

        if (usocklnd_validate_tunables())
                return -EINVAL;

        if (usock_tuns.ut_npollthreads == 0) {
                usock_tuns.ut_npollthreads = cfs_online_cpus();

                if (usock_tuns.ut_npollthreads <= 0) {
                        CERROR("Cannot find out the number of online CPUs\n");
                        return -EINVAL;
                }
        }

        return 0;
}


int
usocklnd_base_startup()
{
        usock_pollthread_t *pt;
        int                 i;
        int                 rc;

        rc = usocklnd_update_tunables();
        if (rc)
                return rc;

        usock_data.ud_npollthreads = usock_tuns.ut_npollthreads;

        LIBCFS_ALLOC (usock_data.ud_pollthreads,
                      usock_data.ud_npollthreads *
                      sizeof(usock_pollthread_t));
        if (usock_data.ud_pollthreads == NULL)
                return -ENOMEM;

        /* Initialize poll thread state structures */
        for (i = 0; i < usock_data.ud_npollthreads; i++) {
                int notifier[2];

                pt = &usock_data.ud_pollthreads[i];

                rc = -ENOMEM;

                LIBCFS_ALLOC (pt->upt_pollfd,
                              sizeof(struct pollfd) * UPT_START_SIZ);
                if (pt->upt_pollfd == NULL)
                        goto base_startup_failed_0;

                LIBCFS_ALLOC (pt->upt_idx2conn,
                              sizeof(usock_conn_t *) * UPT_START_SIZ);
                if (pt->upt_idx2conn == NULL)
                        goto base_startup_failed_1;

                LIBCFS_ALLOC (pt->upt_fd2idx,
                              sizeof(int) * UPT_START_SIZ);
                if (pt->upt_fd2idx == NULL)
                        goto base_startup_failed_2;

                memset(pt->upt_fd2idx, 0,
                       sizeof(int) * UPT_START_SIZ);

                LIBCFS_ALLOC (pt->upt_skip,
                              sizeof(int) * UPT_START_SIZ);
                if (pt->upt_skip == NULL)
                        goto base_startup_failed_3;

                pt->upt_npollfd = pt->upt_nfd2idx = UPT_START_SIZ;

                rc = libcfs_socketpair(notifier);
                if (rc != 0)
                        goto base_startup_failed_4;

                pt->upt_notifier_fd = notifier[0];

                pt->upt_pollfd[0].fd = notifier[1];
                pt->upt_pollfd[0].events = POLLIN;
                pt->upt_pollfd[0].revents = 0;

#if defined(__sun__) || defined(__sun)
                pt->upt_dp_fd = open("/dev/poll", O_RDWR);
                if (pt->upt_dp_fd < 0) {
                        rc = -errno;
                        goto base_startup_failed_5;
                }

                rc = write(pt->upt_dp_fd,
                           pt->upt_pollfd,
                           sizeof(struct pollfd));

                if (rc != sizeof(struct pollfd)) {
                        if (rc <0)
                                rc = -errno;
                        else
                                rc = -EIO;

                        close(pt->upt_dp_fd);
                        goto base_startup_failed_5;
                }
                rc = 0; /* if we're here, write() was OK */

                LIBCFS_ALLOC (pt->upt_requests,
                              sizeof(struct pollfd) * UPT_START_SIZ);
                if (pt->upt_requests == NULL)
                        goto base_startup_failed_6;

                LIBCFS_ALLOC (pt->upt_results,
                              sizeof(struct pollfd) * UPT_START_SIZ);
                if (pt->upt_results == NULL)
                        goto base_startup_failed_7;

                pt->upt_nrequests = 0;
#endif

                pt->upt_nfds = 1;
                pt->upt_idx2conn[0] = NULL;

                pt->upt_errno = 0;
                CFS_INIT_LIST_HEAD (&pt->upt_pollrequests);
                CFS_INIT_LIST_HEAD (&pt->upt_stale_list);
                pthread_mutex_init(&pt->upt_pollrequests_lock, NULL);
                cfs_init_completion(&pt->upt_completion);
        }

        /* Initialize peer hash list */
        for (i = 0; i < UD_PEER_HASH_SIZE; i++)
                CFS_INIT_LIST_HEAD(&usock_data.ud_peers[i]);

        pthread_rwlock_init(&usock_data.ud_peers_lock, NULL);

        /* Spawn poll threads */
        for (i = 0; i < usock_data.ud_npollthreads; i++) {
                rc = cfs_create_thread(usocklnd_poll_thread,
                                       &usock_data.ud_pollthreads[i],
				       "usklndplthr%d", i);
                if (rc) {
                        usocklnd_base_shutdown(i);
                        return rc;
                }
        }

        usock_data.ud_state = UD_STATE_INITIALIZED;

        return 0;

#if defined(__sun__) || defined(__sun)
  base_startup_failed_7:
        LIBCFS_FREE (pt->upt_requests, sizeof(struct pollfd) * UPT_START_SIZ);
  base_startup_failed_6:
        close(pt->upt_dp_fd);
  base_startup_failed_5:
        close(pt->upt_notifier_fd);
        close(pt->upt_pollfd[0].fd);
#endif
  base_startup_failed_4:
        LIBCFS_FREE (pt->upt_skip, sizeof(int) * UPT_START_SIZ);
  base_startup_failed_3:
        LIBCFS_FREE (pt->upt_fd2idx, sizeof(int) * UPT_START_SIZ);
  base_startup_failed_2:
        LIBCFS_FREE (pt->upt_idx2conn, sizeof(usock_conn_t *) * UPT_START_SIZ);
  base_startup_failed_1:
        LIBCFS_FREE (pt->upt_pollfd, sizeof(struct pollfd) * UPT_START_SIZ);
  base_startup_failed_0:
        LASSERT(rc != 0);
        usocklnd_release_poll_states(i);
        LIBCFS_FREE (usock_data.ud_pollthreads,
                     usock_data.ud_npollthreads *
                     sizeof(usock_pollthread_t));
        return rc;
}

void
usocklnd_base_shutdown(int n)
{
        int i;

        usock_data.ud_shutdown = 1;
        for (i = 0; i < n; i++) {
                usock_pollthread_t *pt = &usock_data.ud_pollthreads[i];
                usocklnd_wakeup_pollthread(i);
                cfs_wait_for_completion(&pt->upt_completion);
        }

        pthread_rwlock_destroy(&usock_data.ud_peers_lock);

        usocklnd_release_poll_states(usock_data.ud_npollthreads);

        LIBCFS_FREE (usock_data.ud_pollthreads,
                     usock_data.ud_npollthreads *
                     sizeof(usock_pollthread_t));

        usock_data.ud_state = UD_STATE_INIT_NOTHING;
}

__u64
usocklnd_new_incarnation()
{
        struct timeval tv;
        int            rc = gettimeofday(&tv, NULL);
        LASSERT (rc == 0);
        return (((__u64)tv.tv_sec) * 1000000) + tv.tv_usec;
}

static int
usocklnd_assign_ni_nid(lnet_ni_t *ni)
{
        int   rc;
        int   up;
        __u32 ipaddr;

        /* Find correct IP-address and update ni_nid with it.
         * Two cases are supported:
         * 1) no explicit interfaces are defined. NID will be assigned to
         * first non-lo interface that is up;
         * 2) exactly one explicit interface is defined. For example,
         * LNET_NETWORKS='tcp(eth0)' */

        if (ni->ni_interfaces[0] == NULL) {
                char **names;
                int    i, n;

                n = libcfs_ipif_enumerate(&names);
                if (n <= 0) {
                        CERROR("Can't enumerate interfaces: %d\n", n);
                        return -1;
                }

                for (i = 0; i < n; i++) {

                        if (!strcmp(names[i], "lo") || /* skip the loopback IF */
                            !strcmp(names[i], "lo0"))
                                continue;

                        rc = libcfs_ipif_query(names[i], &up, &ipaddr);
                        if (rc != 0) {
                                CWARN("Can't get interface %s info: %d\n",
                                      names[i], rc);
                                continue;
                        }

                        if (!up) {
                                CWARN("Ignoring interface %s (down)\n",
                                      names[i]);
                            continue;
                        }

                        break;      /* one address is quite enough */
                }

                libcfs_ipif_free_enumeration(names, n);

                if (i >= n) {
                        CERROR("Can't find any usable interfaces\n");
                        return -1;
                }

                CDEBUG(D_NET, "No explicit interfaces defined. "
                       "%u.%u.%u.%u used\n", HIPQUAD(ipaddr));
        } else {
                if (ni->ni_interfaces[1] != NULL) {
                        CERROR("only one explicit interface is allowed\n");
                        return -1;
                }

                rc = libcfs_ipif_query(ni->ni_interfaces[0], &up, &ipaddr);
                if (rc != 0) {
                        CERROR("Can't get interface %s info: %d\n",
                               ni->ni_interfaces[0], rc);
                        return -1;
                }

                if (!up) {
                        CERROR("Explicit interface defined: %s but is down\n",
                               ni->ni_interfaces[0]);
                        return -1;
                }

                CDEBUG(D_NET, "Explicit interface defined: %s. "
                       "%u.%u.%u.%u used\n",
                       ni->ni_interfaces[0], HIPQUAD(ipaddr));

        }

        ni->ni_nid = LNET_MKNID(LNET_NIDNET(ni->ni_nid), ipaddr);

        return 0;
}

int
usocklnd_startup(lnet_ni_t *ni)
{
        int          rc;
        usock_net_t *net;

        if (usock_data.ud_state == UD_STATE_INIT_NOTHING) {
                rc = usocklnd_base_startup();
                if (rc != 0)
                        return rc;
        }

        LIBCFS_ALLOC(net, sizeof(*net));
        if (net == NULL)
                goto startup_failed_0;

        memset(net, 0, sizeof(*net));
        net->un_incarnation = usocklnd_new_incarnation();
        pthread_mutex_init(&net->un_lock, NULL);
        pthread_cond_init(&net->un_cond, NULL);

        ni->ni_data = net;

        if (!(the_lnet.ln_pid & LNET_PID_USERFLAG)) {
                rc = usocklnd_assign_ni_nid(ni);
                if (rc != 0)
                        goto startup_failed_1;
        }

        LASSERT (ni->ni_lnd == &the_tcplnd);

        ni->ni_maxtxcredits = usock_tuns.ut_txcredits;
        ni->ni_peertxcredits = usock_tuns.ut_peertxcredits;

        usock_data.ud_nets_count++;
        return 0;

 startup_failed_1:
        pthread_mutex_destroy(&net->un_lock);
        pthread_cond_destroy(&net->un_cond);
        LIBCFS_FREE(net, sizeof(*net));
 startup_failed_0:
        if (usock_data.ud_nets_count == 0)
                usocklnd_base_shutdown(usock_data.ud_npollthreads);

        return -ENETDOWN;
}

void
usocklnd_shutdown(lnet_ni_t *ni)
{
        usock_net_t *net = ni->ni_data;

        net->un_shutdown = 1;

        usocklnd_del_all_peers(ni);

        /* Wait for all peer state to clean up */
        pthread_mutex_lock(&net->un_lock);
        while (net->un_peercount != 0)
                pthread_cond_wait(&net->un_cond, &net->un_lock);
        pthread_mutex_unlock(&net->un_lock);

        /* Release usock_net_t structure */
        pthread_mutex_destroy(&net->un_lock);
        pthread_cond_destroy(&net->un_cond);
        LIBCFS_FREE(net, sizeof(*net));

        usock_data.ud_nets_count--;
        if (usock_data.ud_nets_count == 0)
                usocklnd_base_shutdown(usock_data.ud_npollthreads);
}

void
usocklnd_del_all_peers(lnet_ni_t *ni)
{
        struct list_head  *ptmp;
        struct list_head  *pnxt;
        usock_peer_t      *peer;
        int                i;

        pthread_rwlock_wrlock(&usock_data.ud_peers_lock);

        for (i = 0; i < UD_PEER_HASH_SIZE; i++) {
                list_for_each_safe (ptmp, pnxt, &usock_data.ud_peers[i]) {
                        peer = list_entry (ptmp, usock_peer_t, up_list);

                        if (peer->up_ni != ni)
                                continue;

                        usocklnd_del_peer_and_conns(peer);
                }
        }

        pthread_rwlock_unlock(&usock_data.ud_peers_lock);

        /* wakeup all threads */
        for (i = 0; i < usock_data.ud_npollthreads; i++)
                usocklnd_wakeup_pollthread(i);
}

void
usocklnd_del_peer_and_conns(usock_peer_t *peer)
{
        /* peer cannot disappear because it's still in hash list */

        pthread_mutex_lock(&peer->up_lock);
        /* content of conn[] array cannot change now */
        usocklnd_del_conns_locked(peer);
        pthread_mutex_unlock(&peer->up_lock);

        /* peer hash list is still protected by the caller */
        list_del(&peer->up_list);

        usocklnd_peer_decref(peer); /* peer isn't in hash list anymore */
}

void
usocklnd_del_conns_locked(usock_peer_t *peer)
{
        int i;

        for (i=0; i < N_CONN_TYPES; i++) {
                usock_conn_t *conn = peer->up_conns[i];
                if (conn != NULL)
                        usocklnd_conn_kill(conn);
        }
}

int
usocklnd_ninstances(void)
{
	return (usock_data.ud_nets_count);
}

int
usocklnd_get_usesdp(void)
{
	return (usock_tuns.ut_usesdp);
}

int
lnet_get_usesdp(void)
{
	return (usock_tuns.ut_usesdp);
}
