/* $Id$ */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <err.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "pfl/cdefs.h"
#include "psc_util/fmt.h"
#include "psc_util/iostats.h"
#include "psc_util/log.h"
#include "psc_util/strlcpy.h"
#include "psc_util/thread.h"
#include "psc_util/timerthr.h"

#include "sdp_inet.h"

/* I/O operation flags */
#define IOF_RD		0			/* read(2) I/O */
#define IOF_WR		(1 << 0)		/* write(2) instead of read(2) */
#define Q		15			/* listen(2) queue length */
#define PORT		15420			/* IPv4 port */

#define THRT_TINTV	0			/* intv timer thread type */
#define THRT_TIOS	1			/* iostats timer thread type */
#define THRT_DISPLAY	2			/* stats displayer */

struct sockarg {
	int s;					/* socket */
};

int			 forcesdp;		/* force sockets direct */
const char		*progname;
char			*buf;
size_t			 bufsiz = 1024 * 1024;
struct iostats	 	 rdst;			/* read stats */
struct iostats	 	 wrst;			/* write stats */
pthread_t		 rdthr;
in_port_t		 port = PORT;

void
center(const char *s, int width)
{
	int len;

	len = (width - strlen(s)) / 2;
	printf("%*s%s%*s", len, "", s, len, "");
}

__dead void *
displaythr_main(__unusedx void *arg)
{
	char ratebuf[PSCFMT_HUMAN_BUFSIZ];
	struct iostats myist;

	center("-- read --", 8 * 3);
	printf("\t|\t");
	center("-- write --", 8 * 3);
	printf("\n"
	    "%7s\t%7s\t%7s\t\t|\t"
	    "%7s\t%7s\t%7s\n",
	    "time", "intvamt", "total",
	    "time", "intvamt", "total");
	printf("================================="
	    "==============================\n");
	for (;;) {
		psc_waitq_wait(&psc_timerwtq, NULL);

		memcpy(&myist, &rdst, sizeof(myist));
		psc_fmt_human(ratebuf, myist.ist_rate);
		printf("\r%6.2fs\t%7s\t",
		    myist.ist_intv.tv_sec +
		    myist.ist_intv.tv_usec * 1e-6,
		    ratebuf);
		psc_fmt_human(ratebuf, myist.ist_bytes_total);
		printf("%7s\t\t|\t", ratebuf);

		memcpy(&myist, &wrst, sizeof(myist));
		psc_fmt_human(ratebuf, myist.ist_rate);
		printf("%6.2fs\t%7s\t",
		    myist.ist_intv.tv_sec +
		    myist.ist_intv.tv_usec * 1e-6,
		    ratebuf);
		psc_fmt_human(ratebuf, myist.ist_bytes_total);
		printf("%7s", ratebuf);

		fflush(stdout);
		sched_yield();
	}
}

__dead void
ioloop(int s, int ioflags)
{
	struct iostats *ist;
	int wr, rem;
	ssize_t rv;
	fd_set set;

	wr = ioflags & IOF_WR;

	if (wr)
		pscthr_init(THRT_DISPLAY, 0, displaythr_main,
		    NULL, 0, "displaythr");

	ist = wr ? &wrst : &rdst;
	for (;;) {
		/* Specifically test that select(2) works. */
		FD_ZERO(&set);
		FD_SET(s, &set);
		if (wr)
			rv = select(s + 1, NULL, &set, NULL, NULL);
		else
			rv = select(s + 1, &set, NULL, NULL, NULL);

		if (rv == -1)
			psc_fatal("select");
		if (rv != 1)
			psc_fatalx("select: unexpected value (%zd)", rv);

		/* transfer a chunk of data */
		rem = bufsiz;
		do {
			if (wr)
				rv = send(s, buf, rem, MSG_NOSIGNAL | MSG_WAITALL);
			else
				rv = recv(s, buf, rem, MSG_NOSIGNAL | MSG_WAITALL);
			if (rv == -1)
				psc_fatal("%s", wr ? "send" : "recv");
			else if (rv == 0)
				psc_fatalx("%s: reached EOF", wr ? "send" : "recv");

			/* tally amount transferred */
			atomic_add(rv, &ist->ist_bytes_intv);
			rem -= rv;
		} while (rem);
	}
}

__dead void *
worker_main(void *arg)
{
	struct sockarg *sarg = arg;

	ioloop(sarg->s, IOF_RD);
}

__dead void
dolisten(const char *listenif)
{
	int opt, rc, s, clisock;
	struct sockaddr_storage ss;
	struct sockaddr_in *sin;
	struct sockarg sarg;
	struct ifreq ifr;
	char addrbuf[50];
	socklen_t salen;

	memset(&ifr, 0, sizeof(ifr));
	if (strlcpy(ifr.ifr_name, listenif,
	    sizeof(ifr.ifr_name)) >= sizeof(ifr.ifr_name))
		psc_fatalx("interface name too long: %s", listenif);
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (ioctl(s, SIOCGIFADDR, &ifr) == -1)
		psc_fatal("ioctl GIFADDR %s", listenif);
	close(s);

	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	if (sin->sin_family != AF_INET)
		psc_fatalx("non-ipv4 not supported");

	if (forcesdp)
		sin->sin_family = AF_INET_SDP;
	sin->sin_port = htons(port);

	s = socket(sin->sin_family, SOCK_STREAM, 0);
	opt = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
	    &opt, sizeof(opt)) == -1)
		psc_fatal("setsockopt");
	if (bind(s, (struct sockaddr *)sin, sizeof(*sin)) == -1)
		psc_fatal("bind");
	if (listen(s, Q) == -1)
		psc_fatal("bind");
	printf("listening on %s:%d\n", inet_ntop(AF_INET,
	    &sin->sin_addr.s_addr, addrbuf, sizeof(addrbuf)),
	    ntohs(sin->sin_port));
	salen = sizeof(ss);
	clisock = accept(s, (struct sockaddr *)&ss, &salen);
	close(s);

	sin = (struct sockaddr_in *)&ss;
	if (sin->sin_family != AF_INET &&
	    sin->sin_family != AF_INET_SDP)
		psc_fatalx("accept: impossible address family %d",
		    sin->sin_family);

	printf("accepted connection from %s:%d\n", inet_ntop(AF_INET,
	    &sin->sin_addr.s_addr, addrbuf, sizeof(addrbuf)),
	    ntohs(sin->sin_port));

	opt = 1;
	if (setsockopt(clisock, IPPROTO_TCP,
	    TCP_NODELAY, &opt, sizeof(opt)) == -1)
		psc_fatal("setsockopt");
	opt = bufsiz;
	if (setsockopt(clisock, SOL_SOCKET, SO_SNDBUF,
	    &opt, sizeof(opt)) == -1)
		psc_fatal("setsockopt");
	opt = bufsiz;
	if (setsockopt(clisock, SOL_SOCKET, SO_RCVBUF,
	    &opt, sizeof(opt)) == -1)
		psc_fatal("setsockopt");

	memset(&sarg, 0, sizeof(sarg));
	sarg.s = clisock;
	if ((rc = pthread_create(&rdthr, NULL, worker_main, &sarg)) != 0)
		psc_fatalx("pthread_create: %s", strerror(rc));
	ioloop(clisock, IOF_WR);
}

__dead void
doconnect(const char *addr)
{
	struct sockaddr_storage ss;
	struct sockaddr_in *sin;
	struct sockarg sarg;
	char addrbuf[50];
	int rc, s;

	memset(&ss, 0, sizeof(ss));
	sin = (struct sockaddr_in *)&ss;
	if (inet_pton(AF_INET, addr, &sin->sin_addr.s_addr) != 1)
		psc_fatal("inet_pton");
	sin->sin_port = htons(port);
	sin->sin_family = AF_INET;

	s = socket(forcesdp ? AF_INET_SDP : AF_INET, SOCK_STREAM, 0);
	if (s == -1)
		psc_fatal("socket");
	if (connect(s, (struct sockaddr *)sin, sizeof(*sin)) == -1)
		psc_fatal("connect");
	printf("connected to %s:%d\n", inet_ntop(sin->sin_family,
	    &sin->sin_addr.s_addr, addrbuf, sizeof(addrbuf)),
	    ntohs(sin->sin_port));

	memset(&sarg, 0, sizeof(sarg));
	sarg.s = s;
	if ((rc = pthread_create(&rdthr, NULL, worker_main, &sarg)) != 0)
		psc_fatalx("pthread_create: %s", strerror(rc));
	ioloop(s, IOF_WR);
}

void
setport(const char *sp)
{
	long l;
	char *p;

	l = strtol(sp, &p, 10);
	if (l < 0 || l >= USHRT_MAX ||
	    p == sp || *p != '\0')
		errx(1, "invalid port: %s", sp);
	port = (in_port_t)l;
}

__dead void
usage(void)
{
	fprintf(stderr, "usage:"
	    "\t%s [-S] [-p port] -l if\n"
	    "\t%s [-S] [-p port] addr\n",
	    progname, progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *listenif;
	char *p;
	int c;

	listenif = NULL;
	progname = argv[0];
	while ((c = getopt(argc, argv, "l:p:S")) != -1)
		switch (c) {
		case 'l':
			listenif = optarg;
			break;
		case 'p':
			setport(optarg);
			break;
		case 'S':
			forcesdp = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (listenif) {
		if (argc)
			usage();
	} else {
		if (argc != 1)
			usage();
	}

	pfl_init();
	buf = PSCALLOC(bufsiz);

	psc_timerthr_spawn(THRT_TINTV, "tintvthr");
	pscthr_init(THRT_TIOS, 0, psc_timer_iosthr_main,
	    NULL, 0, "tiosthr");

	iostats_init(&rdst, "read");
	iostats_init(&wrst, "write");

	if (listenif)
		dolisten(listenif);
	else {
		if ((p = strchr(argv[0], ':')) != NULL) {
			*p++ = '\0';
			setport(p);
		}
		doconnect(argv[0]);
	}
}
