/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.sun.com/software/products/lustre/docs/GPLv2.pdf
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright  2008 Sun Microsystems, Inc. All rights reserved
 * Use is subject to license terms.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre is a trademark of Sun Microsystems, Inc.
 */

#define DEBUG_SUBSYSTEM S_LNET
#include <lnet/lib-lnet.h>

typedef struct {                                /* tmp struct for parsing routes */
	struct list_head   ltb_list;		/* stash on lists */
	int                ltb_size;		/* allocated size */
	char               ltb_text[0];		/* text buffer */
} lnet_text_buf_t;

static int lnet_tbnob = 0;			/* track text buf allocation */
#define LNET_MAX_TEXTBUF_NOB     (64<<10)	/* bound allocation */
#define LNET_SINGLE_TEXTBUF_NOB  (4<<10)

typedef struct {
        struct list_head   lre_list;            /* stash in a list */
        int                lre_min;             /* min value */
        int                lre_max;             /* max value */
        int                lre_stride;          /* stride */
} lnet_range_expr_t;

static int lnet_re_alloc = 0;                   /* track expr allocation */

void
lnet_syntax(char *name, char *str, int offset, int width)
{
        static char dots[LNET_SINGLE_TEXTBUF_NOB];
        static char dashes[LNET_SINGLE_TEXTBUF_NOB];

        memset(dots, '.', sizeof(dots));
        dots[sizeof(dots)-1] = 0;
        memset(dashes, '-', sizeof(dashes));
        dashes[sizeof(dashes)-1] = 0;

	LCONSOLE_ERROR_MSG(0x10f, "Error parsing '%s=\"%s\"'\n", name, str);
	LCONSOLE_ERROR_MSG(0x110, "here...........%.*s..%.*s|%.*s|\n",
                           (int)strlen(name), dots, offset, dots,
                            (width < 1) ? 0 : width - 1, dashes);
}

int
lnet_issep (char c)
{
	switch (c) {
	case '\n':
	case '\r':
	case ';':
		return 1;
	default:
		return 0;
	}
}

int
lnet_iswhite (char c)
{
	switch (c) {
	case ' ':
	case '\t':
	case '\n':
	case '\r':
		return 1;
	default:
		return 0;
	}
}

char *
lnet_trimwhite(char *str)
{
	char *end;

	while (lnet_iswhite(*str))
		str++;

	end = str + strlen(str);
	while (end > str) {
		if (!lnet_iswhite(end[-1]))
			break;
		end--;
	}

	*end = 0;
	return str;
}

int
lnet_net_unique(__u32 net, struct list_head *nilist)
{
        struct list_head *tmp;
        lnet_ni_t        *ni;

        list_for_each (tmp, nilist) {
                ni = list_entry(tmp, lnet_ni_t, ni_list);

                if (LNET_NIDNET(ni->ni_nid) == net)
                        return 0;
        }

        return 1;
}

lnet_ni_t *
lnet_new_ni(__u32 net, struct list_head *nilist)
{
        lnet_ni_t *ni;
	char buf[32];

        if (!lnet_net_unique(net, nilist)) {
                LCONSOLE_ERROR_MSG(0x111, "Duplicate network specified: %s\n",
                                   libcfs_net2str(net));
                return NULL;
        }

        LIBCFS_ALLOC(ni, sizeof(*ni));
        if (ni == NULL) {
                CERROR("Out of memory creating network %s\n",
                       libcfs_net2str(net));
                return NULL;
        }

        /* zero counters/flags, NULL pointers... */
        memset(ni, 0, sizeof(*ni));

	libcfs_net2str2(net, buf);
	psc_iostats_init(&ni->ni_send_ist, "lni-rcv-%s", buf);
	psc_iostats_init(&ni->ni_recv_ist, "lni-snd-%s", buf);

        /* LND will fill in the address part of the NID */
        ni->ni_nid = LNET_MKNID(net, 0);
        CFS_INIT_LIST_HEAD(&ni->ni_txq);

        list_add_tail(&ni->ni_list, nilist);
        return ni;
}

int
lnet_parse_networks(struct list_head *nilist, char *networks)
{
	int        tokensize = strlen(networks) + 1;
        char      *tokens;
        char      *str;
        lnet_ni_t *ni;
        __u32      net;
        int        nnets = 0;

	if (strlen(networks) > LNET_SINGLE_TEXTBUF_NOB) {
		/* _WAY_ conservative */
		LCONSOLE_ERROR_MSG(0x112, "Can't parse networks: string too "
                                   "long\n");
		return -EINVAL;
	}

        LIBCFS_ALLOC(tokens, tokensize);
        if (tokens == NULL) {
                CERROR("Can't allocate net tokens\n");
		return -ENOMEM;
        }

        the_lnet.ln_network_tokens = tokens;
        the_lnet.ln_network_tokens_nob = tokensize;
        memcpy (tokens, networks, tokensize);
	str = tokens;

        /* Add in the loopback network */
        ni = lnet_new_ni(LNET_MKNET(LOLND, 0), nilist);
        if (ni == NULL)
                goto failed;
	ni->ni_interfaces[0] = CFS_LOOPBACK_IFNAME; 

        while (str != NULL && *str != 0) {
                char      *comma = strchr(str, ',');
                char      *bracket = strchr(str, '(');
                int        niface;
		char      *iface;

                /* NB we don't check interface conflicts here; it's the LNDs
                 * responsibility (if it cares at all) */

                if (bracket == NULL ||
		    (comma != NULL && comma < bracket)) {

                        /* no interface list specified */

			if (comma != NULL)
				*comma++ = 0;
			net = libcfs_str2net(lnet_trimwhite(str));

			if (net == LNET_NIDNET(LNET_NID_ANY)) {
                                lnet_syntax("networks", networks,
                                            str - tokens, strlen(str));
                                LCONSOLE_ERROR_MSG(0x113, "Unrecognised network"
                                                   " type\n");
                                goto failed;
                        }

                        if (LNET_NETTYP(net) != LOLND && /* loopback is implicit */
                            lnet_new_ni(net, nilist) == NULL)
                                goto failed;

			str = comma;
			continue;
		}

		*bracket = 0;
		net = libcfs_str2net(lnet_trimwhite(str));
		if (net == LNET_NIDNET(LNET_NID_ANY)) {
                        lnet_syntax("networks", networks,
                                    str - tokens, strlen(str));
                        goto failed;
                }

                if (nnets > 0 &&
                    the_lnet.ln_ptlcompat > 0) {
                        LCONSOLE_ERROR_MSG(0x114, "Only 1 network supported when"
                                           " 'portals_compatible' is set\n");
                        goto failed;
                }

                nnets++;
                ni = lnet_new_ni(net, nilist);
                if (ni == NULL)
                        goto failed;

                niface = 0;
		iface = bracket + 1;

		bracket = strchr(iface, ')');
		if (bracket == NULL) {
                        lnet_syntax("networks", networks,
                                    iface - tokens, strlen(iface));
                        goto failed;
		}

		*bracket = 0;
		do {
			comma = strchr(iface, ',');
			if (comma != NULL)
				*comma++ = 0;

			iface = lnet_trimwhite(iface);
			if (*iface == 0) {
                                lnet_syntax("networks", networks,
                                            iface - tokens, strlen(iface));
                                goto failed;
                        }

                        if (niface == LNET_MAX_INTERFACES) {
                                LCONSOLE_ERROR_MSG(0x115, "Too many interfaces "
                                                   "for net %s\n",
                                                   libcfs_net2str(net));
                                goto failed;
                        }

                        ni->ni_interfaces[niface++] = iface;
			iface = comma;
		} while (iface != NULL);

		str = bracket + 1;
		comma = strchr(bracket + 1, ',');
		if (comma != NULL) {
			*comma = 0;
			str = lnet_trimwhite(str);
			if (*str != 0) {
                                lnet_syntax("networks", networks,
                                            str - tokens, strlen(str));
                                goto failed;
                        }
			str = comma + 1;
			continue;
		}

		str = lnet_trimwhite(str);
		if (*str != 0) {
                        lnet_syntax("networks", networks,
                                    str - tokens, strlen(str));
                        goto failed;
                }
	}

        LASSERT (!list_empty(nilist));
        return 0;

 failed:
        while (!list_empty(nilist)) {
                ni = list_entry(nilist->next, lnet_ni_t, ni_list);

                list_del(&ni->ni_list);
		psc_iostats_remove(&ni->ni_send_ist);
		psc_iostats_remove(&ni->ni_recv_ist);
                LIBCFS_FREE(ni, sizeof(*ni));
        }
	LIBCFS_FREE(tokens, tokensize);
        the_lnet.ln_network_tokens = NULL;

        return -EINVAL;
}

lnet_text_buf_t *
lnet_new_text_buf (int str_len)
{
	lnet_text_buf_t *ltb;
	int              nob;

        /* NB allocate space for the terminating 0 */
	nob = offsetof(lnet_text_buf_t, ltb_text[str_len + 1]);
	if (nob > LNET_SINGLE_TEXTBUF_NOB) {
		/* _way_ conservative for "route net gateway..." */
		CERROR("text buffer too big\n");
		return NULL;
	}

	if (lnet_tbnob + nob > LNET_MAX_TEXTBUF_NOB) {
		CERROR("Too many text buffers\n");
		return NULL;
	}

	LIBCFS_ALLOC(ltb, nob);
	if (ltb == NULL)
		return NULL;

	ltb->ltb_size = nob;
        ltb->ltb_text[0] = 0;
	lnet_tbnob += nob;
	return ltb;
}

void
lnet_free_text_buf (lnet_text_buf_t *ltb)
{
	lnet_tbnob -= ltb->ltb_size;
	LIBCFS_FREE(ltb, ltb->ltb_size);
}

void
lnet_free_text_bufs(struct list_head *tbs)
{
	lnet_text_buf_t  *ltb;

	while (!list_empty(tbs)) {
		ltb = list_entry(tbs->next, lnet_text_buf_t, ltb_list);

		list_del(&ltb->ltb_list);
		lnet_free_text_buf(ltb);
	}
}

void
lnet_print_text_bufs(struct list_head *tbs)
{
	struct list_head  *tmp;
	lnet_text_buf_t   *ltb;

	list_for_each (tmp, tbs) {
		ltb = list_entry(tmp, lnet_text_buf_t, ltb_list);

		CDEBUG(D_WARNING, "%s\n", ltb->ltb_text);
	}

	CDEBUG(D_WARNING, "%d allocated\n", lnet_tbnob);
}

int
lnet_str2tbs_sep (struct list_head *tbs, char *str)
{
	struct list_head  pending;
	char             *sep;
	int               nob;
        int               i;
	lnet_text_buf_t  *ltb;

	CFS_INIT_LIST_HEAD(&pending);

	/* Split 'str' into separate commands */
	for (;;) {
                /* skip leading whitespace */
                while (lnet_iswhite(*str))
                        str++;

		/* scan for separator or comment */
		for (sep = str; *sep != 0; sep++)
			if (lnet_issep(*sep) || *sep == '#')
				break;

		nob = sep - str;
		if (nob > 0) {
			ltb = lnet_new_text_buf(nob);
			if (ltb == NULL) {
				lnet_free_text_bufs(&pending);
				return -1;
			}

                        for (i = 0; i < nob; i++)
                                if (lnet_iswhite(str[i]))
                                        ltb->ltb_text[i] = ' ';
                                else
                                        ltb->ltb_text[i] = str[i];

			ltb->ltb_text[nob] = 0;

			list_add_tail(&ltb->ltb_list, &pending);
		}

		if (*sep == '#') {
			/* scan for separator */
			do {
				sep++;
			} while (*sep != 0 && !lnet_issep(*sep));
		}

		if (*sep == 0)
			break;

		str = sep + 1;
	}

	list_splice(&pending, tbs->prev);
	return 0;
}

int
lnet_expand1tb (struct list_head *list,
	       char *str, char *sep1, char *sep2,
	       char *item, int itemlen)
{
	int              len1 = sep1 - str;
	int              len2 = strlen(sep2 + 1);
	lnet_text_buf_t *ltb;

	LASSERT (*sep1 == '[');
	LASSERT (*sep2 == ']');

	ltb = lnet_new_text_buf(len1 + itemlen + len2);
	if (ltb == NULL)
		return -ENOMEM;

	memcpy(ltb->ltb_text, str, len1);
	memcpy(&ltb->ltb_text[len1], item, itemlen);
	memcpy(&ltb->ltb_text[len1+itemlen], sep2 + 1, len2);
	ltb->ltb_text[len1 + itemlen + len2] = 0;

	list_add_tail(&ltb->ltb_list, list);
	return 0;
}

int
lnet_str2tbs_expand (struct list_head *tbs, char *str)
{
	char              num[16];
	struct list_head  pending;
	char             *sep;
	char             *sep2;
	char             *parsed;
	char             *enditem;
	int               lo;
	int               hi;
	int               stride;
	int               i;
	int               nob;
	int               scanned;

	CFS_INIT_LIST_HEAD(&pending);

	sep = strchr(str, '[');
	if (sep == NULL)			/* nothing to expand */
		return 0;

        sep2 = strchr(sep, ']');
        if (sep2 == NULL)
                goto failed;

	for (parsed = sep; parsed < sep2; parsed = enditem) {

		enditem = ++parsed;
		while (enditem < sep2 && *enditem != ',')
			enditem++;

		if (enditem == parsed)		/* no empty items */
			goto failed;

                if (sscanf(parsed, "%d-%d/%d%n", &lo, &hi, &stride, &scanned) < 3) {

			if (sscanf(parsed, "%d-%d%n", &lo, &hi, &scanned) < 2) {

				/* simple string enumeration */
				if (lnet_expand1tb(&pending, str, sep, sep2,
                                                   parsed, enditem - parsed) != 0)
					goto failed;

				continue;
			}

			stride = 1;
		}

		/* range expansion */

		if (enditem != parsed + scanned) /* no trailing junk */
			goto failed;

		if (hi < 0 || lo < 0 || stride < 0 || hi < lo ||
		    (hi - lo) % stride != 0)
			goto failed;

		for (i = lo; i <= hi; i += stride) {

			snprintf(num, sizeof(num), "%d", i);
			nob = strlen(num);
			if (nob + 1 == sizeof(num))
				goto failed;

			if (lnet_expand1tb(&pending, str, sep, sep2,
                                           num, nob) != 0)
				goto failed;
		}
	}

	list_splice(&pending, tbs->prev);
	return 1;

 failed:
	lnet_free_text_bufs(&pending);
	return -1;
}

int
lnet_parse_hops (char *str, unsigned int *hops)
{
        int     len = strlen(str);
        int     nob = len;

        return (sscanf(str, "%u%n", hops, &nob) >= 1 &&
                nob == len &&
                *hops > 0 && *hops < 256);
}


int
lnet_parse_route (char *str, int *im_a_router)
{
	/* static scratch buffer OK (single threaded) */
	static char       cmd[LNET_SINGLE_TEXTBUF_NOB];

	struct list_head  nets;
	struct list_head  gateways;
	struct list_head *tmp1;
	struct list_head *tmp2;
	__u32             net;
	lnet_nid_t        nid;
	lnet_text_buf_t  *ltb;
	int               rc;
	char             *sep;
	char             *token = str;
	int               ntokens = 0;
        int               myrc = -1;
        unsigned int      hops;
        int               got_hops = 0;

	CFS_INIT_LIST_HEAD(&gateways);
	CFS_INIT_LIST_HEAD(&nets);

	/* save a copy of the string for error messages */
	strncpy(cmd, str, sizeof(cmd) - 1);
	cmd[sizeof(cmd) - 1] = 0;

	sep = str;
	for (;;) {
		/* scan for token start */
		while (lnet_iswhite(*sep))
			sep++;
		if (*sep == 0) {
			if (ntokens < (got_hops ? 3 : 2))
                                goto token_error;
			break;
		}

		ntokens++;
		token = sep++;

		/* scan for token end */
		while (*sep != 0 && !lnet_iswhite(*sep))
			sep++;
		if (*sep != 0)
			*sep++ = 0;

		if (ntokens == 1) {
			tmp2 = &nets;		/* expanding nets */
                } else if (ntokens == 2 &&
                           lnet_parse_hops(token, &hops)) {
                        got_hops = 1;           /* got a hop count */
                        continue;
                } else {
			tmp2 = &gateways;	/* expanding gateways */
                }

		ltb = lnet_new_text_buf(strlen(token));
		if (ltb == NULL)
			goto out;

		strcpy(ltb->ltb_text, token);
		tmp1 = &ltb->ltb_list;
		list_add_tail(tmp1, tmp2);

		while (tmp1 != tmp2) {
			ltb = list_entry(tmp1, lnet_text_buf_t, ltb_list);

			rc = lnet_str2tbs_expand(tmp1->next, ltb->ltb_text);
			if (rc < 0)
				goto token_error;

			tmp1 = tmp1->next;

			if (rc > 0) {		/* expanded! */
				list_del(&ltb->ltb_list);
				lnet_free_text_buf(ltb);
				continue;
			}

			if (ntokens == 1) {
				net = libcfs_str2net(ltb->ltb_text);
				if (net == LNET_NIDNET(LNET_NID_ANY) ||
                                    LNET_NETTYP(net) == LOLND)
					goto token_error;
			} else {
				nid = libcfs_str2nid(ltb->ltb_text);
				if (nid == LNET_NID_ANY ||
                                    LNET_NETTYP(LNET_NIDNET(nid)) == LOLND)
					goto token_error;
			}
		}
	}

        if (!got_hops)
                hops = 1;

	LASSERT (!list_empty(&nets));
	LASSERT (!list_empty(&gateways));

	list_for_each (tmp1, &nets) {
		ltb = list_entry(tmp1, lnet_text_buf_t, ltb_list);
		net = libcfs_str2net(ltb->ltb_text);
		LASSERT (net != LNET_NIDNET(LNET_NID_ANY));

		list_for_each (tmp2, &gateways) {
			ltb = list_entry(tmp2, lnet_text_buf_t, ltb_list);
			nid = libcfs_str2nid(ltb->ltb_text);
			LASSERT (nid != LNET_NID_ANY);

                        if (lnet_islocalnid(nid)) {
                                *im_a_router = 1;
                                continue;
                        }

                        rc = lnet_add_route (net, hops, nid);
                        if (rc != 0) {
                                CERROR("Can't create route "
                                       "to %s via %s\n",
                                       libcfs_net2str(net),
                                       libcfs_nid2str(nid));
                                goto out;
                        }
		}
	}

        myrc = 0;
        goto out;

 token_error:
	lnet_syntax("routes", cmd, token - str, strlen(token));
 out:
	lnet_free_text_bufs(&nets);
	lnet_free_text_bufs(&gateways);
	return myrc;
}

int
lnet_parse_route_tbs(struct list_head *tbs, int *im_a_router)
{
	lnet_text_buf_t   *ltb;

	while (!list_empty(tbs)) {
		ltb = list_entry(tbs->next, lnet_text_buf_t, ltb_list);

		if (lnet_parse_route(ltb->ltb_text, im_a_router) < 0) {
			lnet_free_text_bufs(tbs);
			return -EINVAL;
		}

		list_del(&ltb->ltb_list);
		lnet_free_text_buf(ltb);
	}

        return 0;
}

int
lnet_parse_routes (char *routes, int *im_a_router)
{
	struct list_head  tbs;
	int               rc = 0;

        *im_a_router = 0;

        if (the_lnet.ln_ptlcompat > 0 &&
            routes[0] != 0) {
                /* Can't route when running in compatibility mode */
                LCONSOLE_ERROR_MSG(0x116, "Route tables are not supported when "
                                   "'portals_compatible' is set\n");
                return -EINVAL;
        }

	CFS_INIT_LIST_HEAD(&tbs);

	if (lnet_str2tbs_sep(&tbs, routes) < 0) {
		CERROR("Error parsing routes\n");
		rc = -EINVAL;
	} else {
                rc = lnet_parse_route_tbs(&tbs, im_a_router);
        }

	LASSERT (lnet_tbnob == 0);
	return rc;
}

void
lnet_print_range_exprs(struct list_head *exprs)
{
        struct list_head   *e;
        lnet_range_expr_t *lre;

        list_for_each(e, exprs) {
                lre = list_entry(exprs->next, lnet_range_expr_t, lre_list);

                CDEBUG(D_WARNING, "%d-%d/%d\n",
                       lre->lre_min, lre->lre_max, lre->lre_stride);
        }

        CDEBUG(D_WARNING, "%d allocated\n", lnet_re_alloc);
}

int
lnet_new_range_expr(struct list_head *exprs, int min, int max, int stride)
{
        lnet_range_expr_t *lre;

        CDEBUG(D_NET, "%d-%d/%d\n", min, max, stride);

        if (min < 0 || min > 255 || min > max || stride < 0)
                return -EINVAL;

        LIBCFS_ALLOC(lre, sizeof(*lre));
        if (lre == NULL)
                return -ENOMEM;

        lnet_re_alloc++;

        lre->lre_min = min;
        lre->lre_max = max;
        lre->lre_stride = stride;

        list_add(&lre->lre_list, exprs);
        return 0;
}

void
lnet_destroy_range_exprs(struct list_head *exprs)
{
        lnet_range_expr_t *lre;

        while (!list_empty(exprs)) {
                lre = list_entry(exprs->next, lnet_range_expr_t, lre_list);

                list_del(&lre->lre_list);
                LIBCFS_FREE(lre, sizeof(*lre));
                lnet_re_alloc--;
        }
}

int
lnet_parse_range_expr(struct list_head *exprs, char *str)
{
        int                nob = strlen(str);
        char              *sep;
        int                n;
        int                x;
        int                y;
        int                z;
        int                rc;

        if (nob == 0)
                return -EINVAL;

        if (!strcmp(str, "*"))                  /* match all */
                return lnet_new_range_expr(exprs, 0, 255, 1);

        n = nob;
        if (sscanf(str, "%u%n", &x, &n) >= 1 && n == nob) {
                /* simple number */
                return lnet_new_range_expr(exprs, x, x, 1);
        }

        /* Has to be an expansion */
        if (!(str[0] == '[' && nob > 2 && str[nob-1] == ']'))
                return -EINVAL;

        nob -= 2;
        str++;
        str[nob] = 0;

        do {
                /* Comma separated list of expressions... */
                sep = strchr(str, ',');
                if (sep != NULL)
                        *sep++ = 0;

                nob = strlen(str);
                n = nob;
                if (sscanf(str, "%u%n", &x, &n) >= 1 && n == nob) {
                        /* simple number */
                        rc = lnet_new_range_expr(exprs, x, x, 1);
                        if (rc != 0)
                                return rc;

                        continue;
                }

                n = nob;
                if (sscanf(str, "%u-%u%n", &x, &y, &n) >= 2 && n == nob) {
                        /* simple range */
                        rc = lnet_new_range_expr(exprs, x, y, 1);
                        if (rc != 0)
                                return rc;
                        continue;
                }

                n = nob;
                if (sscanf(str, "%u-%u/%u%n", &x, &y, &z, &n) >= 3 && n == nob) {
                        /* strided range */
                        rc = lnet_new_range_expr(exprs, x, y, z);
                        if (rc != 0)
                                return rc;
                        continue;
                }

                return -EINVAL;

        } while ((str = sep) != NULL);

        return 0;
}

int
lnet_match_network_token(char *token, __u32 *ipaddrs, int nip)
{
        struct list_head   exprs[4];
        struct list_head  *e;
        lnet_range_expr_t *re;
        char              *str;
        int                i;
        int                j;
        __u32              ip;
        int                n;
        int                match;
        int                rc;

        for (i = 0; i < 4; i++)
                CFS_INIT_LIST_HEAD(&exprs[i]);

        for (i = 0; i < 4; i++) {
                str = token;
                if (i != 3) {
                        token = strchr(token, '.');
                        if (token == NULL) {
                                rc = -EINVAL;
                                goto out;
                        }
                        *token++ = 0;
                }

                rc = lnet_parse_range_expr(&exprs[i], str);
                if (rc != 0) {
                        LASSERT (rc < 0);
                        goto out;
                }
        }

        for (match = i = 0; !match && i < nip; i++) {
                ip = ipaddrs[i];

                for (match = 1, j = 0; match && j < 4; j++) {
                        n = (ip >> (8 * (3 - j))) & 0xff;
                        match = 0;

                        list_for_each(e, &exprs[j]) {
                                re = list_entry(e, lnet_range_expr_t, lre_list);

                                if (re->lre_min <= n &&
                                    re->lre_max >= n &&
                                    (n - re->lre_min) % re->lre_stride == 0) {
                                        match = 1;
                                        break;
                                }
                        }
                }
        }

        rc = match ? 1 : 0;

 out:
        for (i = 0; i < 4; i++)
                lnet_destroy_range_exprs(&exprs[i]);
        LASSERT (lnet_re_alloc == 0);

        return rc;
}

int
lnet_match_network_tokens(char *net_entry, __u32 *ipaddrs, int nip)
{
        static char tokens[LNET_SINGLE_TEXTBUF_NOB];

        int   matched = 0;
        int   ntokens = 0;
        int   len;
        char *net = NULL;
        char *sep;
        char *token;
        int   rc;

        LASSERT (strlen(net_entry) < sizeof(tokens));

        /* work on a copy of the string */
        strcpy(tokens, net_entry);
        sep = tokens;
        for (;;) {
                /* scan for token start */
                while (lnet_iswhite(*sep))
                        sep++;
                if (*sep == 0)
                        break;

                token = sep++;

                /* scan for token end */
                while (*sep != 0 && !lnet_iswhite(*sep))
                        sep++;
                if (*sep != 0)
                        *sep++ = 0;

                if (ntokens++ == 0) {
                        net = token;
                        continue;
                }

                len = strlen(token);

                rc = lnet_match_network_token(token, ipaddrs, nip);
                if (rc < 0) {
                        lnet_syntax("ip2nets", net_entry,
                                    token - tokens, len);
                        return rc;
                }

                matched |= (rc != 0);
        }

        if (!matched)
                return 0;

        strcpy(net_entry, net);                 /* replace with matched net */
        return 1;
}

__u32
lnet_netspec2net(char *netspec)
{
        char   *bracket = strchr(netspec, '(');
        __u32   net;

        if (bracket != NULL)
                *bracket = 0;

        net = libcfs_str2net(netspec);

        if (bracket != NULL)
                *bracket = '(';

        return net;
}

int
lnet_splitnets(char *source, struct list_head *nets)
{
        int               offset = 0;
        int               offset2;
        int               len;
        lnet_text_buf_t  *tb;
        lnet_text_buf_t  *tb2;
        struct list_head *t;
        char             *sep;
        char             *bracket;
        __u32             net;

        LASSERT (!list_empty(nets));
        LASSERT (nets->next == nets->prev);     /* single entry */

        tb = list_entry(nets->next, lnet_text_buf_t, ltb_list);

        for (;;) {
                sep = strchr(tb->ltb_text, ',');
                bracket = strchr(tb->ltb_text, '(');

                if (sep != NULL &&
                    bracket != NULL &&
                    bracket < sep) {
                        /* netspec lists interfaces... */

                        offset2 = offset + (bracket - tb->ltb_text);
                        len = strlen(bracket);

                        bracket = strchr(bracket + 1, ')');

                        if (bracket == NULL ||
                            !(bracket[1] == ',' || bracket[1] == 0)) {
                                lnet_syntax("ip2nets", source, offset2, len);
                                return -EINVAL;
                        }

                        sep = (bracket[1] == 0) ? NULL : bracket + 1;
                }

                if (sep != NULL)
                        *sep++ = 0;

                net = lnet_netspec2net(tb->ltb_text);
                if (net == LNET_NIDNET(LNET_NID_ANY)) {
                        lnet_syntax("ip2nets", source, offset,
                                    strlen(tb->ltb_text));
                        return -EINVAL;
                }

                list_for_each(t, nets) {
                        tb2 = list_entry(t, lnet_text_buf_t, ltb_list);

                        if (tb2 == tb)
                                continue;

                        if (net == lnet_netspec2net(tb2->ltb_text)) {
                                /* duplicate network */
                                lnet_syntax("ip2nets", source, offset,
                                            strlen(tb->ltb_text));
                                return -EINVAL;
                        }
                }

                if (sep == NULL)
                        return 0;

                offset += sep - tb->ltb_text;
                tb2 = lnet_new_text_buf(strlen(sep));
                if (tb2 == NULL)
                        return -ENOMEM;

                strcpy(tb2->ltb_text, sep);
                list_add_tail(&tb2->ltb_list, nets);

                tb = tb2;
        }
}

int
lnet_match_networks (char **networksp, char *ip2nets, __u32 *ipaddrs, int nip)
{
        static char  networks[LNET_SINGLE_TEXTBUF_NOB];
        static char  source[LNET_SINGLE_TEXTBUF_NOB];

        struct list_head    raw_entries;
        struct list_head    matched_nets;
        struct list_head    current_nets;
        struct list_head   *t;
        struct list_head   *t2;
        lnet_text_buf_t    *tb;
        lnet_text_buf_t    *tb2;
        __u32               net1;
        __u32               net2;
        int                 len;
        int                 count;
        int                 dup;
        int                 rc;

        CFS_INIT_LIST_HEAD(&raw_entries);
        if (lnet_str2tbs_sep(&raw_entries, ip2nets) < 0) {
                CERROR("Error parsing ip2nets\n");
                LASSERT (lnet_tbnob == 0);
                return -EINVAL;
        }

        CFS_INIT_LIST_HEAD(&matched_nets);
        CFS_INIT_LIST_HEAD(&current_nets);
        networks[0] = 0;
        count = 0;
        len = 0;
        rc = 0;

        while (!list_empty(&raw_entries)) {
                tb = list_entry(raw_entries.next, lnet_text_buf_t, ltb_list);

                strncpy(source, tb->ltb_text, sizeof(source)-1);
                source[sizeof(source)-1] = 0;

                /* replace ltb_text with the network(s) add on match */
                rc = lnet_match_network_tokens(tb->ltb_text, ipaddrs, nip);
                if (rc < 0)
                        break;

                list_del(&tb->ltb_list);

                if (rc == 0) {                  /* no match */
                        lnet_free_text_buf(tb);
                        continue;
                }

                /* split into separate networks */
                CFS_INIT_LIST_HEAD(&current_nets);
                list_add(&tb->ltb_list, &current_nets);
                rc = lnet_splitnets(source, &current_nets);
                if (rc < 0)
                        break;

                dup = 0;
                list_for_each (t, &current_nets) {
                        tb = list_entry(t, lnet_text_buf_t, ltb_list);
                        net1 = lnet_netspec2net(tb->ltb_text);
                        LASSERT (net1 != LNET_NIDNET(LNET_NID_ANY));

                        list_for_each(t2, &matched_nets) {
                                tb2 = list_entry(t2, lnet_text_buf_t, ltb_list);
                                net2 = lnet_netspec2net(tb2->ltb_text);
                                LASSERT (net2 != LNET_NIDNET(LNET_NID_ANY));

                                if (net1 == net2) {
                                        dup = 1;
                                        break;
                                }
                        }

                        if (dup)
                                break;
                }

                if (dup) {
                        lnet_free_text_bufs(&current_nets);
                        continue;
                }

                list_for_each_safe(t, t2, &current_nets) {
                        tb = list_entry(t, lnet_text_buf_t, ltb_list);

                        list_del(&tb->ltb_list);
                        list_add_tail(&tb->ltb_list, &matched_nets);

                        len += snprintf(networks + len, sizeof(networks) - len,
                                        "%s%s", (len == 0) ? "" : ",",
                                        tb->ltb_text);

                        if (len >= (int)sizeof(networks)) {
                                CERROR("Too many matched networks\n");
                                rc = -E2BIG;
                                goto out;
                        }
                }

                count++;
        }

 out:
        lnet_free_text_bufs(&raw_entries);
        lnet_free_text_bufs(&matched_nets);
        lnet_free_text_bufs(&current_nets);
        LASSERT (lnet_tbnob == 0);

        if (rc < 0)
                return rc;

        *networksp = networks;
        return count;
}

#ifdef __KERNEL__
void
lnet_ipaddr_free_enumeration(__u32 *ipaddrs, int nip)
{
        LIBCFS_FREE(ipaddrs, nip * sizeof(*ipaddrs));
}

int
lnet_ipaddr_enumerate (__u32 **ipaddrsp)
{
        int        up;
        __u32      netmask;
        __u32     *ipaddrs;
        __u32     *ipaddrs2;
        int        nip;
        char     **ifnames;
        int        nif = libcfs_ipif_enumerate(&ifnames);
        int        i;
        int        rc;

        if (nif <= 0)
                return nif;

        LIBCFS_ALLOC(ipaddrs, nif * sizeof(*ipaddrs));
        if (ipaddrs == NULL) {
                CERROR("Can't allocate ipaddrs[%d]\n", nif);
                libcfs_ipif_free_enumeration(ifnames, nif);
                return -ENOMEM;
        }

        for (i = nip = 0; i < nif; i++) {
                if (!strcmp(ifnames[i], CFS_LOOPBACK_IFNAME))
                        continue;

                rc = libcfs_ipif_query(ifnames[i], &up,
                                       &ipaddrs[nip], &netmask);
                if (rc != 0) {
                        CWARN("Can't query interface %s: %d\n",
                              ifnames[i], rc);
                        continue;
                }

                if (!up) {
                        CWARN("Ignoring interface %s: it's down\n",
                              ifnames[i]);
                        continue;
                }

                nip++;
        }

        libcfs_ipif_free_enumeration(ifnames, nif);

        if (nip == nif) {
                *ipaddrsp = ipaddrs;
        } else {
                if (nip > 0) {
                        LIBCFS_ALLOC(ipaddrs2, nip * sizeof(*ipaddrs2));
                        if (ipaddrs2 == NULL) {
                                CERROR("Can't allocate ipaddrs[%d]\n", nip);
                                nip = -ENOMEM;
                        } else {
                                memcpy(ipaddrs2, ipaddrs,
                                       nip * sizeof(*ipaddrs));
                                *ipaddrsp = ipaddrs2;
                                rc = nip;
                        }
                }
                lnet_ipaddr_free_enumeration(ipaddrs, nif);
        }
        return nip;
}

int
lnet_parse_ip2nets (char **networksp, char *ip2nets)
{
        __u32     *ipaddrs;
        int        nip = lnet_ipaddr_enumerate(&ipaddrs);
        int        rc;

        if (nip < 0) {
                LCONSOLE_ERROR_MSG(0x117, "Error %d enumerating local IP "
                                   "interfaces for ip2nets to match\n", nip);
                return nip;
        }

        if (nip == 0) {
                LCONSOLE_ERROR_MSG(0x118, "No local IP interfaces "
                                   "for ip2nets to match\n");
                return -ENOENT;
        }

        rc = lnet_match_networks(networksp, ip2nets, ipaddrs, nip);
        lnet_ipaddr_free_enumeration(ipaddrs, nip);

        if (rc < 0) {
                LCONSOLE_ERROR_MSG(0x119, "Error %d parsing ip2nets\n", rc);
                return rc;
        }

        if (rc == 0) {
                LCONSOLE_ERROR_MSG(0x11a, "ip2nets does not match "
                                   "any local IP interfaces\n");
                return -ENOENT;
        }

        return 0;
}

int
lnet_set_ip_niaddr (lnet_ni_t *ni)
{
        __u32  net = LNET_NIDNET(ni->ni_nid);
        char **names;
        int    n;
        __u32  ip;
        __u32  netmask;
        int    up;
        int    i;
        int    rc;

        /* Convenience for LNDs that use the IP address of a local interface as
         * the local address part of their NID */

        if (ni->ni_interfaces[0] != NULL) {

                CLASSERT (LNET_MAX_INTERFACES > 1);

                if (ni->ni_interfaces[1] != NULL) {
                        CERROR("Net %s doesn't support multiple interfaces\n",
                               libcfs_net2str(net));
                        return -EPERM;
                }

                rc = libcfs_ipif_query(ni->ni_interfaces[0],
                                       &up, &ip, &netmask);
                if (rc != 0) {
                        CERROR("Net %s can't query interface %s: %d\n",
                               libcfs_net2str(net), ni->ni_interfaces[0], rc);
                        return -EPERM;
                }

                if (!up) {
                        CERROR("Net %s can't use interface %s: it's down\n",
                               libcfs_net2str(net), ni->ni_interfaces[0]);
                        return -ENETDOWN;
                }

                ni->ni_nid = LNET_MKNID(net, ip);
                return 0;
        }

        n = libcfs_ipif_enumerate(&names);
        if (n <= 0) {
                CERROR("Net %s can't enumerate interfaces: %d\n",
                       libcfs_net2str(net), n);
                return 0;
        }

        for (i = 0; i < n; i++) {
                if (!strcmp(names[i], CFS_LOOPBACK_IFNAME)) /* skip the loopback IF */
                        continue;

                rc = libcfs_ipif_query(names[i], &up, &ip, &netmask);

                if (rc != 0) {
                        CWARN("Net %s can't query interface %s: %d\n",
                              libcfs_net2str(net), names[i], rc);
                        continue;
                }

                if (!up) {
                        CWARN("Net %s ignoring interface %s (down)\n",
                              libcfs_net2str(net), names[i]);
                        continue;
                }

                libcfs_ipif_free_enumeration(names, n);
                ni->ni_nid = LNET_MKNID(net, ip);
                return 0;
        }

        CERROR("Net %s can't find any interfaces\n", libcfs_net2str(net));
        libcfs_ipif_free_enumeration(names, n);
        return -ENOENT;
}
EXPORT_SYMBOL(lnet_set_ip_niaddr);

#endif
