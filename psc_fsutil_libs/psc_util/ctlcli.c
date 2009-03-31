/* $Id$ */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "psc_ds/list.h"
#include "psc_ds/pool.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/cdefs.h"
#include "psc_util/ctl.h"
#include "psc_util/ctlcli.h"
#include "psc_util/fmt.h"
#include "psc_util/fmtstr.h"
#include "psc_util/log.h"
#include "psc_util/meter.h"
#include "psc_util/strlcpy.h"
#include "psc_util/subsys.h"

__static PSCLIST_HEAD(psc_ctlmsgs);

struct psc_ctlmsg {
	struct psclist_head	pcm_lentry;
	struct psc_ctlmsghdr	pcm_mh;
};

int psc_ctl_noheader;
int psc_ctl_inhuman;
int psc_ctl_nsubsys;
char **psc_ctl_subsys_names;
__static int psc_ctl_lastmsgtype = -1;

__static struct psc_ctlshow_ent *
psc_ctlshow_lookup(const char *name)
{
	int n;

	if (strlen(name) == 0)
		return (NULL);
	for (n = 0; n < psc_ctlshow_ntabents; n++)
		if (strncasecmp(name, psc_ctlshow_tab[n].pse_name,
		    strlen(name)) == 0)
			return (&psc_ctlshow_tab[n]);
	return (NULL);
}

void *
psc_ctlmsg_push(int type, size_t msiz)
{
	struct psc_ctlmsg *pcm;
	static int id;
	size_t tsiz;

	tsiz = msiz + sizeof(*pcm);
	pcm = psc_alloc(tsiz, PAF_NOLOG);
	psclist_xadd_tail(&pcm->pcm_lentry, &psc_ctlmsgs);
	pcm->pcm_mh.mh_type = type;
	pcm->pcm_mh.mh_size = msiz;
	pcm->pcm_mh.mh_id = id++;
	return (&pcm->pcm_mh.mh_data);
}

void
psc_ctlparse_hashtable(const char *tblname)
{
	struct psc_ctlmsg_hashtable *pcht;

	pcht = psc_ctlmsg_push(PCMT_GETHASHTABLE, sizeof(*pcht));
	strlcpy(pcht->pcht_name, tblname, sizeof(pcht->pcht_name));
}

void
psc_ctl_packshow_loglevel(const char *thr)
{
	struct psc_ctlmsg_loglevel *pcl;
	int n;

	psc_ctlmsg_push(PCMT_GETSUBSYS,
	    sizeof(struct psc_ctlmsg_subsys));

	pcl = psc_ctlmsg_push(PCMT_GETLOGLEVEL, sizeof(*pcl));
	n = strlcpy(pcl->pcl_thrname, thr, sizeof(pcl->pcl_thrname));
	if (n == 0 || n >= (int)sizeof(pcl->pcl_thrname))
		psc_fatalx("invalid thread name: %s", thr);
}

void
psc_ctl_packshow_stats(const char *thr)
{
	struct psc_ctlmsg_stats *pcst;
	int n;

	pcst = psc_ctlmsg_push(PCMT_GETSTATS, sizeof(*pcst));
	n = strlcpy(pcst->pcst_thrname, thr, sizeof(pcst->pcst_thrname));
	if (n == 0 || n >= (int)sizeof(pcst->pcst_thrname))
		psc_fatalx("invalid thread name: %s", thr);
}

void
psc_ctlparse_show(char *showspec)
{
	char *thrlist, *thr, *thrnext;
	struct psc_ctlshow_ent *pse;

	if ((thrlist = strchr(showspec, ':')) == NULL)
		thrlist = PCTHRNAME_EVERYONE;
	else
		*thrlist++ = '\0';

	if ((pse = psc_ctlshow_lookup(showspec)) == NULL)
		errx(1, "invalid show parameter: %s", showspec);

	for (thr = thrlist; thr != NULL; thr = thrnext) {
		if ((thrnext = strchr(thr, ',')) != NULL)
			*thrnext++ = '\0';
		pse->pse_cb(thr);
	}
}

void
psc_ctlparse_lc(char *lists)
{
	struct psc_ctlmsg_lc *pclc;
	char *list, *listnext;
	int n;

	for (list = lists; list != NULL; list = listnext) {
		if ((listnext = strchr(list, ',')) != NULL)
			*listnext++ = '\0';

		pclc = psc_ctlmsg_push(PCMT_GETLC, sizeof(*pclc));

		n = snprintf(pclc->pclc_name, sizeof(pclc->pclc_name),
		    "%s", list);
		if (n == -1)
			psc_fatal("snprintf");
		else if (n == 0 || n > (int)sizeof(pclc->pclc_name))
			psc_fatalx("invalid list: %s", list);
	}
}

void
psc_ctlparse_param(char *spec)
{
	struct psc_ctlmsg_param *pcp;
	char *thr, *field, *value;
	int flags, n;

	flags = 0;
	if ((value = strchr(spec, '=')) != NULL) {
		if (value > spec && value[-1] == '+') {
			flags = PCPF_ADD;
			value[-1] = '\0';
		} else if (value > spec && value[-1] == '-') {
			flags = PCPF_SUB;
			value[-1] = '\0';
		}
		*value++ = '\0';
	}

	if ((field = strchr(spec, '.')) == NULL) {
		thr = PCTHRNAME_EVERYONE;
		field = spec;
	} else {
		*field = '\0';

		if (strstr(spec, "thr") == NULL) {
			/*
			 * No thread specified:
			 * assume global or everyone.
			 */
			thr = PCTHRNAME_EVERYONE;
			*field = '.';
			field = spec;
		} else {
			/*
			 * We saw "thr" at the first level:
			 * assume thread specification.
			 */
			thr = spec;
			field++;
		}
	}

	pcp = psc_ctlmsg_push(value ? PCMT_SETPARAM : PCMT_GETPARAM,
	    sizeof(*pcp));

	/* Set thread name. */
	n = snprintf(pcp->pcp_thrname, sizeof(pcp->pcp_thrname), "%s", thr);
	if (n == -1)
		psc_fatal("snprintf");
	else if (n == 0 || n > (int)sizeof(pcp->pcp_thrname))
		psc_fatalx("invalid thread name: %s", thr);

	/* Set parameter name. */
	n = snprintf(pcp->pcp_field, sizeof(pcp->pcp_field),
	    "%s", field);
	if (n == -1)
		psc_fatal("snprintf");
	else if (n == 0 || n > (int)sizeof(pcp->pcp_field))
		psc_fatalx("invalid parameter: %s", thr);

	/* Set parameter value (if applicable). */
	if (value) {
		pcp->pcp_flags = flags;
		n = snprintf(pcp->pcp_value,
		    sizeof(pcp->pcp_value), "%s", value);
		if (n == -1)
			psc_fatal("snprintf");
		else if (n == 0 || n > (int)sizeof(pcp->pcp_value))
			psc_fatalx("invalid parameter value: %s", thr);
	}
}

void
psc_ctlparse_pool(char *pools)
{
	struct psc_ctlmsg_pool *pcpl;
	char *pool, *poolnext;

	for (pool = pools; pool; pool = poolnext) {
		if ((poolnext = strchr(pool, ',')) != NULL)
			*poolnext++ = '\0';

		pcpl = psc_ctlmsg_push(PCMT_GETPOOL, sizeof(*pcpl));
		if (strlcpy(pcpl->pcpl_name, pool,
		    sizeof(pcpl->pcpl_name)) >= sizeof(pcpl->pcpl_name))
			psc_fatalx("invalid pool: %s", pool);
	}
}

void
psc_ctlparse_iostats(char *iostats)
{
	struct psc_ctlmsg_iostats *pci;
	char *iostat, *next;
	int n;

	for (iostat = iostats; iostat != NULL; iostat = next) {
		if ((next = strchr(iostat, ',')) != NULL)
			*next++ = '\0';

		pci = psc_ctlmsg_push(PCMT_GETIOSTATS, sizeof(*pci));

		/* Set iostat name. */
		n = snprintf(pci->pci_ist.ist_name,
		    sizeof(pci->pci_ist.ist_name), "%s", iostat);
		if (n == -1)
			psc_fatal("snprintf");
		else if (n == 0 || n > (int)sizeof(pci->pci_ist.ist_name))
			psc_fatalx("invalid iostat name: %s", iostat);
	}
}

void
psc_ctlparse_meter(char *meters)
{
	struct psc_ctlmsg_meter *pcm;
	char *meter, *next;
	size_t n;

	for (meter = meters; meter != NULL; meter = next) {
		if ((next = strchr(meter, ',')) != NULL)
			*next++ = '\0';

		pcm = psc_ctlmsg_push(PCMT_GETMETER, sizeof(*pcm));
		n = strlcpy(pcm->pcm_mtr.pm_name, meter,
		    sizeof(pcm->pcm_mtr.pm_name));
		if (n == 0 || n >= sizeof(pcm->pcm_mtr.pm_name))
			psc_fatalx("invalid meter: %s", meter);
	}
}

void
psc_ctlparse_mlist(char *mlists)
{
	struct psc_ctlmsg_mlist *pcml;
	char *mlist, *mlistnext;
	int n;

	for (mlist = mlists; mlist != NULL; mlist = mlistnext) {
		if ((mlistnext = strchr(mlist, ',')) != NULL)
			*mlistnext++ = '\0';

		pcml = psc_ctlmsg_push(PCMT_GETMLIST, sizeof(*pcml));

		n = snprintf(pcml->pcml_name, sizeof(pcml->pcml_name),
		    "%s", mlist);
		if (n == -1)
			psc_fatal("snprintf");
		else if (n == 0 || n > (int)sizeof(pcml->pcml_name))
			psc_fatalx("invalid mlist: %s", mlist);
	}
}

void
psc_ctlparse_cmd(char *cmd)
{
	struct psc_ctlmsg_cmd *pcc;
	int i;

	for (i = 0; i < psc_ctlcmd_nreqs; i++)
		if (strcasecmp(cmd, psc_ctlcmd_reqs[i].pccr_name) == 0) {
			pcc = psc_ctlmsg_push(PCMT_CMD, sizeof(*pcc));
			pcc->pcc_opcode = psc_ctlcmd_reqs[i].pccr_opcode;
			return;
		}
	psc_fatalx("command not found: %s", cmd);
}

int
psc_ctl_loglevel_namelen(int n)
{
	size_t maxlen;
	int j;

	maxlen = strlen(psc_ctl_subsys_names[n]);
	for (j = 0; j < PNLOGLEVELS; j++)
		maxlen = MAX(maxlen, strlen(psc_loglevel_getname(j)));
	return (maxlen);
}

int
psc_ctlthr_prhdr(void)
{
	return (printf(" %-*s %9s %8s %8s\n", PSC_THRNAME_MAX,
	    "thread", "#nclients", "#sent", "#recv"));
}

void
psc_ctlthr_prdat(const struct psc_ctlmsg_stats *pcst)
{
	printf(" %-*s %9u %8u %8u\n", PSC_THRNAME_MAX,
	    pcst->pcst_thrname, pcst->pcst_nclients,
	    pcst->pcst_nsent, pcst->pcst_nrecv);
}

int
psc_ctlmsg_hashtable_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("hash table statistics\n");
	return (printf(" %-20s %6s %6s %6s "
	    "%6s %6s %6s\n",
	    "table", "used", "total", "%use",
	    "ents", "avglen", "maxlen"));
}

void
psc_ctlmsg_hashtable_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_hashtable *pcht = m;
	char rbuf[PSCFMT_RATIO_BUFSIZ];

	psc_fmt_ratio(rbuf, pcht->pcht_usedbucks, pcht->pcht_totalbucks);
	printf(" %-20s %6d "
	    "%6d %6s %6d "
	    "%6.1f "
	    "%6d\n",
	    pcht->pcht_name, pcht->pcht_usedbucks,
	    pcht->pcht_totalbucks, rbuf, pcht->pcht_nents,
	    pcht->pcht_nents * 1.0 / pcht->pcht_totalbucks,
	    pcht->pcht_maxbucklen);
}

void
psc_ctlmsg_error_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_error *pce = m;

	printf("error: %s\n", pce->pce_errmsg);
}

int
psc_ctlmsg_subsys_check(struct psc_ctlmsghdr *mh, const void *m)
{
	const struct psc_ctlmsg_subsys *pcss = m;
	int n;

	if (mh->mh_size == 0 ||
	    mh->mh_size % PCSS_NAME_MAX)
		return (sizeof(*pcss));

	/* Release old subsystems. */
	for (n = 0; n < psc_ctl_nsubsys; n++)
		PSCFREE(psc_ctl_subsys_names[n]);
	PSCFREE(psc_ctl_subsys_names);

	psc_ctl_nsubsys = mh->mh_size / PCSS_NAME_MAX;
	psc_ctl_subsys_names = PSCALLOC(psc_ctl_nsubsys *
	    sizeof(*psc_ctl_subsys_names));
	for (n = 0; n < psc_ctl_nsubsys; n++) {
		psc_ctl_subsys_names[n] = PSCALLOC(PCSS_NAME_MAX);
		memcpy(psc_ctl_subsys_names[n],
		    &pcss->pcss_names[n * PCSS_NAME_MAX], PCSS_NAME_MAX);
		psc_ctl_subsys_names[n][PCSS_NAME_MAX - 1] = '\0';
	}
	mh->mh_type = psc_ctl_lastmsgtype;	/* hack to fix newline */
	return (0);
}

int
psc_ctlmsg_iostats_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("iostats\n");
	return (printf(" %-37s %10s %12s %8s %8s\n",
	    "name", "ratecur", "total", "erate", "#err"));
}

void
psc_ctlmsg_iostats_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_iostats *pci = m;
	const struct iostats *ist = &pci->pci_ist;
	char buf[PSCFMT_HUMAN_BUFSIZ];

	printf(" %-37s ", ist->ist_name);
	if (psc_ctl_inhuman) {
		printf("%10.2f ", ist->ist_rate);
		printf("%12"PRIu64" ", ist->ist_bytes_total);
	} else {
		psc_fmt_human(buf, ist->ist_rate);
		printf("%8s/s ", buf);

		psc_fmt_human(buf, ist->ist_bytes_total);
		printf("%12s ", buf);
	}
	printf("%6.1f/s %8"PRIu64"\n", ist->ist_erate,
	    ist->ist_errors_total);
}

int
psc_ctlmsg_meter_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("meters\n");
	return (printf(" %12s %13s %8s\n",
	    "name", "position", "progress"));
}

void
psc_ctlmsg_meter_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_meter *pcm = m;
	int n, len;

	len = printf(" %12s %5zu/%8zu ",
	    pcm->pcm_mtr.pm_name,
	    pcm->pcm_mtr.pm_cur,
	    pcm->pcm_mtr.pm_max);
	psc_assert(len != -1);
#define WIDTH 80
	len = WIDTH - len - 3;
	if (len < 0)
		len = 0;
	putchar('|');
	for (n = 0; n < (int)(len * pcm->pcm_mtr.pm_cur /
	    pcm->pcm_mtr.pm_max); n++)
		putchar('=');
	putchar(pcm->pcm_mtr.pm_cur ==
	    pcm->pcm_mtr.pm_max ? '=' : '>');
	for (; n < len; n++)
		putchar(' ');
	printf("|\n");
}

int
psc_ctlmsg_pool_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("pools\n");
	return (printf(
	    " %-8s %3s %6s %6s %6s "
	    "%6s %6s %6s %2s "
	    "%6s %6s %3s %3s\n",
	    "name", "flg", "#free", "#use", "total",
	    "%use", "min", "max", "th",
	    "#grow", "#shrnk", "#em", "#wa"));
	/* XXX add ngets and waiting/sleep time */
}

void
psc_ctlmsg_pool_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_pool *pcpl = m;
	char rbuf[PSCFMT_RATIO_BUFSIZ];

	psc_fmt_ratio(rbuf, pcpl->pcpl_total - pcpl->pcpl_free,
	    pcpl->pcpl_total);
	printf(
	    " %-8s %c%c%c "
	    "%6d %6d "
	    "%6d %6s",
	    pcpl->pcpl_name,
	    pcpl->pcpl_flags & PPMF_AUTO ? 'A' : '-',
	    pcpl->pcpl_flags & PPMF_NOLOCK ? 'N' : '-',
	    pcpl->pcpl_flags & PPMF_MLIST ? 'M' : '-',
	    pcpl->pcpl_free, pcpl->pcpl_total - pcpl->pcpl_free,
	    pcpl->pcpl_total, rbuf);
	if (pcpl->pcpl_flags & PPMF_AUTO) {
		printf(" %6d ", pcpl->pcpl_min);
		if (pcpl->pcpl_max)
			printf("%6d", pcpl->pcpl_max);
		else
			printf("%6s", "<inf>");
		printf(" %2d", pcpl->pcpl_thres);
	} else
		printf(" %6s %6s %2s", "-", "-", "-");

	if (pcpl->pcpl_flags & PPMF_AUTO)
		printf(" %6zu %6zu", pcpl->pcpl_ngrow,
		    pcpl->pcpl_nshrink);
	else
		printf(" %6s %6s", "-", "-");

	printf(" %3d", pcpl->pcpl_nw_empty);
	if (pcpl->pcpl_flags & PPMF_MLIST)
		printf("   -");
	else
		printf(" %3d", pcpl->pcpl_nw_want);
	printf("\n");
}

int
psc_ctlmsg_lc_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("list caches\n");
	return (printf(" %-42s %3s %8s %3s %3s %15s\n",
	    "name", "flg", "#items", "#wa", "#em", "#seen"));
}

void
psc_ctlmsg_lc_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_lc *pclc = m;

	printf(
	    " %-42s   %c "
	    "%8zu %3d %3d %15zu\n",
	    pclc->pclc_name,
	    pclc->pclc_flags & PLCF_DYING ? 'D' : '-',
	    pclc->pclc_size,
	    pclc->pclc_nw_want, pclc->pclc_nw_empty,
	    pclc->pclc_nseen);
}

int
psc_ctlmsg_param_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("parameters\n");
	return (printf(" %-35s %s\n", "name", "value"));
}

void
psc_ctlmsg_param_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_param *pcp = m;

	if (strcmp(pcp->pcp_thrname, PCTHRNAME_EVERYONE) == 0)
		printf(" %-35s %s\n", pcp->pcp_field, pcp->pcp_value);
	else
		printf(" %s.%-*s %s\n", pcp->pcp_thrname,
		    35 - (int)strlen(pcp->pcp_thrname) - 1,
		    pcp->pcp_field, pcp->pcp_value);
}

#define MT_SKIPHDR (-2)
__static int psc_ctl_stat_prthrtype;

int
psc_ctlmsg_stats_check(__unusedx struct psc_ctlmsghdr *mh, const void *m)
{
	static int last_thrtype = -1;

	const struct psc_ctlmsg_stats *pcst = m;

	if (mh->mh_size != sizeof(*pcst))
		return (sizeof(*pcst));

	if (psc_ctl_lastmsgtype == PCMT_GETSTATS) {
		if (last_thrtype != pcst->pcst_thrtype) {
			psc_ctl_stat_prthrtype = 1;
			last_thrtype = pcst->pcst_thrtype;
			psc_ctl_lastmsgtype = MT_SKIPHDR;
		}
	} else
		last_thrtype = -1;
	return (0);
}

int
psc_ctlmsg_stats_prhdr(__unusedx struct psc_ctlmsghdr *mh, const void *m)
{
	const struct psc_ctlmsg_stats *pcst = m;
	struct psc_ctl_thrstatfmt *ptf;

	/* Check thread type. */
	if (pcst->pcst_thrtype < 0 ||
	    pcst->pcst_thrtype >= psc_ctl_nthrstatfmts)
		psc_fatalx("invalid thread type: %d",
		    pcst->pcst_thrtype);

	/* Skip thread types for which there are no print routine. */
	ptf = &psc_ctl_thrstatfmts[pcst->pcst_thrtype];
	if (ptf->ptf_prhdr == NULL)
		psc_fatalx("invalid thread type: %d",
		    pcst->pcst_thrtype);

	if (psc_ctl_lastmsgtype != MT_SKIPHDR)
		printf("thread stats\n");
	return (ptf->ptf_prhdr());
}

void
psc_ctlmsg_stats_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_stats *pcst = m;
	struct psc_ctl_thrstatfmt *ptf;

	/* Skip thread types for which there are no print routine. */
	ptf = &psc_ctl_thrstatfmts[pcst->pcst_thrtype];
	if (ptf->ptf_prdat)
		ptf->ptf_prdat(pcst);
}

int
psc_ctlmsg_loglevel_check(struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	__unusedx struct psc_ctlmsg_loglevel *pcl;

	if (mh->mh_size != sizeof(*pcl) +
	    psc_ctl_nsubsys * sizeof(*pcl->pcl_levels))
		return (sizeof(*pcl));
	return (0);
}

int
psc_ctlmsg_loglevel_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	int len, n;

	printf("logging levels\n");
	len = printf(" %-*s ", PSC_THRNAME_MAX, "thread");
	for (n = 0; n < psc_ctl_nsubsys; n++)
		len += printf(" %*s", psc_ctl_loglevel_namelen(n),
		    psc_ctl_subsys_names[n]);
	len += printf("\n");
	return (len);
}

void
psc_ctlmsg_loglevel_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_loglevel *pcl = m;
	int n;

	printf(" %-*s ", PSC_THRNAME_MAX, pcl->pcl_thrname);
	for (n = 0; n < psc_ctl_nsubsys; n++)
		printf(" %*s", psc_ctl_loglevel_namelen(n),
		    psc_loglevel_getname(pcl->pcl_levels[n]));
	printf("\n");
}

int
psc_ctlmsg_mlist_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("mlists\n");
	return (printf(" %-50s %8s %3s %15s\n",
	    "name", "#items", "#em", "#seen"));
}

void
psc_ctlmsg_mlist_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_mlist *pcml = m;

	printf(" %-50s %8d %3d %15"PRIu64"\n",
	    pcml->pcml_name, pcml->pcml_size,
	    pcml->pcml_waitors, pcml->pcml_nseen);
}

__static void
psc_ctlmsg_print(struct psc_ctlmsghdr *mh, const void *m)
{
	const struct psc_ctlmsg_prfmt *prf;
	int n, len;

	/* Validate message type. */
	if (mh->mh_type < 0 ||
	    mh->mh_type >= psc_ctlmsg_nprfmts)
		psc_fatalx("invalid ctlmsg type %d", mh->mh_type);
	prf = &psc_ctlmsg_prfmts[mh->mh_type];

	/* Validate message size. */
	if (prf->prf_msgsiz) {
		if (prf->prf_msgsiz != mh->mh_size)
			psc_fatalx("invalid ctlmsg size; type=%d; "
			    "sizeof=%zu expected=%zu", mh->mh_type,
			    mh->mh_size, prf->prf_msgsiz);
	} else if (prf->prf_check == NULL)
		/* Disallowed message type. */
		psc_fatalx("invalid ctlmsg type %d", mh->mh_type);
	else if ((n = prf->prf_check(mh, m)) != 0)
		psc_fatalx("invalid ctlmsg size; type=%d; sizeof=%zu "
		    "expected=%d", mh->mh_type, mh->mh_size, n);

	/* Print display header. */
	if (!psc_ctl_noheader && psc_ctl_lastmsgtype != mh->mh_type &&
	    prf->prf_prhdr != NULL) {
		if (psc_ctl_lastmsgtype != -1)
			printf("\n");
		len = prf->prf_prhdr(mh, m);
		for (n = 0; n < len - 1; n++)
			putchar('=');
		putchar('\n');
	}

	/* Print display contents. */
	if (prf->prf_prdat)
		prf->prf_prdat(mh, m);
	psc_ctl_lastmsgtype = mh->mh_type;
}

void
psc_ctlcli_main(const char *osockfn)
{
	extern void usage(void);

	struct psc_ctlmsg *pcm, *nextpcm;
	struct psc_ctlmsghdr mh;
	struct sockaddr_un sun;
	char sockfn[PATH_MAX];
	ssize_t siz, n;
	void *m;
	int s;

	if (psclist_empty(&psc_ctlmsgs))
		usage();

	FMTSTR(sockfn, sizeof(sockfn), osockfn,
		FMTSTRCASE('h', sockfn, sizeof(sockfn), "s",
		    psclog_getdata()->pld_hostname)
	);

	/* Connect to control socket. */
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		psc_fatal("socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	snprintf(sun.sun_path, sizeof(sun.sun_path), "%s", sockfn);
	if (connect(s, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "connect: %s", sockfn);

	/* Send queued control messages. */
	psclist_for_each_entry_safe(pcm, nextpcm,
	    &psc_ctlmsgs, pcm_lentry) {
		siz = pcm->pcm_mh.mh_size + sizeof(pcm->pcm_mh);
		if (write(s, &pcm->pcm_mh, siz) != siz)
			psc_fatal("write");
		free(pcm);
	}
	if (shutdown(s, SHUT_WR) == -1)
		psc_fatal("shutdown");

	/* Read and print response messages. */
	m = NULL;
	siz = 0;
	while ((n = read(s, &mh, sizeof(mh))) != -1 && n != 0) {
		if (n != sizeof(mh)) {
			psc_warnx("short read");
			continue;
		}
		if (mh.mh_size == 0)
			psc_fatalx("received invalid message from daemon");
		if (mh.mh_size >= (size_t)siz) {
			siz = mh.mh_size;
			if ((m = realloc(m, siz)) == NULL)
				psc_fatal("realloc");
		}
		n = read(s, m, mh.mh_size);
		if (n == -1)
			psc_fatal("read");
		else if (n == 0)
			psc_fatalx("received unexpected EOF from daemon");
		psc_ctlmsg_print(&mh, m);
	}
	if (n == -1)
		psc_fatal("read");
	free(m);
	close(s);
}
