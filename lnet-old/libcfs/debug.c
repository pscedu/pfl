/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2002 Cluster File Systems, Inc.
 *   Author: Phil Schwan <phil@clusterfs.com>
 *
 *   This file is part of Lustre, http://www.lustre.org.
 *
 *   Lustre is free software; you can redistribute it and/or
 *   modify it under the terms of version 2 of the GNU General Public
 *   License as published by the Free Software Foundation.
 *
 *   Lustre is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Lustre; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef EXPORT_SYMTAB
# define EXPORT_SYMTAB
#endif

# define DEBUG_SUBSYSTEM S_LNET

#ifdef __APPLE__
#include <mach/vm_param.h>
#endif

#include <asm/page.h>

#include <libcfs/kp30.h>
#include <libcfs/libcfs.h>
#include "tracefile.h"

#include "psc_util/cdefs.h"

static char debug_file_name[1024];

#ifdef __KERNEL__
unsigned int libcfs_subsystem_debug = ~0;
EXPORT_SYMBOL(libcfs_subsystem_debug);

unsigned int libcfs_debug = (D_EMERG | D_ERROR | D_WARNING | D_CONSOLE |
                             D_NETERROR | D_HA | D_CONFIG | D_IOCTL |
                             D_DLMTRACE | D_RPCTRACE | D_VFSTRACE);
EXPORT_SYMBOL(libcfs_debug);

unsigned int libcfs_printk;
EXPORT_SYMBOL(libcfs_printk);

unsigned int libcfs_console_ratelimit = 1;
EXPORT_SYMBOL(libcfs_console_ratelimit);

unsigned int libcfs_debug_binary = 1;
EXPORT_SYMBOL(libcfs_debug_binary);

unsigned int libcfs_stack;
EXPORT_SYMBOL(libcfs_stack);

unsigned int portal_enter_debugger;
EXPORT_SYMBOL(portal_enter_debugger);

unsigned int libcfs_catastrophe;
EXPORT_SYMBOL(libcfs_catastrophe);

atomic_t libcfs_kmemory = ATOMIC_INIT(0);
EXPORT_SYMBOL(libcfs_kmemory);

static cfs_waitq_t debug_ctlwq;

char debug_file_path[1024] = "/tmp/lustre-log";

int libcfs_panic_in_progress;

/* libcfs_debug_token2mask() expects the returned
 * string in lower-case */
const char *
libcfs_debug_subsys2str(int subsys)
{
        switch (subsys) {
        default:
                return NULL;
        case S_UNDEFINED:
                return "undefined";
        case S_MDC:
                return "mdc";
        case S_MDS:
                return "mds";
        case S_OSC:
                return "osc";
        case S_OST:
                return "ost";
        case S_CLASS:
                return "class";
        case S_LOG:
                return "log";
        case S_LLITE:
                return "llite";
        case S_RPC:
                return "rpc";
        case S_LNET:
                return "lnet";
        case S_LND:
                return "lnd";
        case S_PINGER:
                return "pinger";
        case S_FILTER:
                return "filter";
        case S_ECHO:
                return "echo";
        case S_LDLM:
                return "ldlm";
        case S_LOV:
                return "lov";
        case S_LMV:
                return "lmv";
        case S_SEC:
                return "sec";
        case S_GSS:
                return "gss";
        case S_MGC:
                return "mgc";
        case S_MGS:
                return "mgs";
        case S_FID:
                return "fid";
        case S_FLD:
                return "fld";
        }
}

/* libcfs_debug_token2mask() expects the returned
 * string in lower-case */
const char *
libcfs_debug_dbg2str(int debug)
{
        switch (debug) {
        default:
                return NULL;
        case D_TRACE:
                return "trace";
        case D_INODE:
                return "inode";
        case D_SUPER:
                return "super";
        case D_EXT2:
                return "ext2";
        case D_MALLOC:
                return "malloc";
        case D_CACHE:
                return "cache";
        case D_INFO:
                return "info";
        case D_IOCTL:
                return "ioctl";
        case D_NETERROR:
                return "neterror";
        case D_NET:
                return "net";
        case D_WARNING:
                return "warning";
        case D_BUFFS:
                return "buffs";
        case D_OTHER:
                return "other";
        case D_DENTRY:
                return "dentry";
        case D_PAGE:
                return "page";
        case D_DLMTRACE:
                return "dlmtrace";
        case D_ERROR:
                return "error";
        case D_EMERG:
                return "emerg";
        case D_HA:
                return "ha";
        case D_RPCTRACE:
                return "rpctrace";
        case D_VFSTRACE:
                return "vfstrace";
        case D_READA:
                return "reada";
        case D_MMAP:
                return "mmap";
        case D_CONFIG:
                return "config";
        case D_CONSOLE:
                return "console";
        case D_QUOTA:
                return "quota";
        case D_SEC:
                return "sec";
        }
}

int
libcfs_debug_mask2str(char *str, int size, int mask, int is_subsys)
{
        const char *(*fn)(int bit) = is_subsys ? libcfs_debug_subsys2str :
                                                 libcfs_debug_dbg2str;
        int           len = 0;
        const char   *token;
        int           bit;
        int           i;

        if (mask == 0) {                        /* "0" */
                if (size > 0)
                        str[0] = '0';
                len = 1;
        } else {                                /* space-separated tokens */
                for (i = 0; i < 32; i++) {
                        bit = 1 << i;

                        if ((mask & bit) == 0)
                                continue;

                        token = fn(bit);
                        if (token == NULL)              /* unused bit */
                                continue;

                        if (len > 0) {                  /* separator? */
                                if (len < size)
                                        str[len] = ' ';
                                len++;
                        }
                
                        while (*token != 0) {
                                if (len < size)
                                        str[len] = *token;
                                token++;
                                len++;
                        }
                }
        }

        /* terminate 'str' */
        if (len < size)
                str[len] = 0;
        else
                str[size - 1] = 0;

        return len;
}

int
libcfs_debug_token2mask(int *mask, const char *str, int len, int is_subsys)
{
        const char *(*fn)(int bit) = is_subsys ? libcfs_debug_subsys2str :
                                                 libcfs_debug_dbg2str;
        int           i;
        int           j;
        int           bit;
        const char   *token;

        /* match against known tokens */
        for (i = 0; i < 32; i++) {
                bit = 1 << i;

                token = fn(bit);
                if (token == NULL)              /* unused? */
                        continue;
                
                /* strcasecmp */
                for (j = 0; ; j++) {
                        if (j == len) {         /* end of token */
                                if (token[j] == 0) {
                                        *mask = bit;
                                        return 0;
                                }
                                break;
                        }
                        
                        if (token[j] == 0)
                                break;
                                
                        if (str[j] == token[j])
                                continue;
                        
                        if (str[j] < 'A' || 'Z' < str[j])
                                break;

                        if (str[j] - 'A' + 'a' != token[j])
                                break;
                }
        }
        
        return -EINVAL;                         /* no match */
}

int
libcfs_debug_str2mask(int *mask, const char *str, int is_subsys)
{
        int         m = 0;
        int         matched = 0;
        char        op = 0;
        int         n;
        int         t;

        /* <str> must be a list of debug tokens or numbers separated by
         * whitespace and optionally an operator ('+' or '-').  If an operator
         * appears first in <str>, '*mask' is used as the starting point
         * (relative), otherwise 0 is used (absolute).  An operator applies to
         * all following tokens up to the next operator. */
        
        while (*str != 0) {
                while (isspace(*str)) /* skip whitespace */
                        str++;

                if (*str == 0)
                        break;

                if (*str == '+' || *str == '-') {
                        op = *str++;

                        /* op on first token == relative */
                        if (!matched)
                                m = *mask;

                        while (isspace(*str)) /* skip whitespace */
                                str++;

                        if (*str == 0)          /* trailing op */
                                return -EINVAL;
                }

                /* find token length */
                for (n = 0; str[n] != 0 && !isspace(str[n]); n++);

                /* match token */
                if (libcfs_debug_token2mask(&t, str, n, is_subsys) != 0)
                        return -EINVAL;
                
                matched = 1;
                if (op == '-')
                        m &= ~t;
                else
                        m |= t;
                
                str += n;
        }

        if (!matched)
                return -EINVAL;

        *mask = m;
        return 0;
}

void libcfs_debug_dumplog_internal(void *arg)
{
        CFS_DECL_JOURNAL_DATA;

        CFS_PUSH_JOURNAL;

        snprintf(debug_file_name, sizeof(debug_file_path) - 1, "%s.%ld.%ld",
                 debug_file_path, cfs_time_current_sec(), (long)arg);
        printk(KERN_ALERT "LustreError: dumping log to %s\n", debug_file_name);
        tracefile_dump_all_pages(debug_file_name);

        CFS_POP_JOURNAL;
}

int libcfs_debug_dumplog_thread(void *arg)
{
        cfs_daemonize("");
        libcfs_debug_dumplog_internal(arg);
        cfs_waitq_signal(&debug_ctlwq);
        return 0;
}

void libcfs_debug_dumplog(void)
{
        int            rc;
        cfs_waitlink_t wait;
        ENTRY;

        /* we're being careful to ensure that the kernel thread is
         * able to set our state to running as it exits before we
         * get to schedule() */
        cfs_waitlink_init(&wait);
        set_current_state(TASK_INTERRUPTIBLE);
        cfs_waitq_add(&debug_ctlwq, &wait);

        rc = cfs_kernel_thread(libcfs_debug_dumplog_thread,
                               (void *)(long)cfs_curproc_pid(),
                               CLONE_VM | CLONE_FS | CLONE_FILES);
        if (rc < 0)
                printk(KERN_ERR "LustreError: cannot start log dump thread: "
                       "%d\n", rc);
        else
                cfs_waitq_wait(&wait, CFS_TASK_INTERRUPTIBLE);

        /* be sure to teardown if kernel_thread() failed */
        cfs_waitq_del(&debug_ctlwq, &wait);
        set_current_state(TASK_RUNNING);
}

int libcfs_debug_init(unsigned long bufsize)
{
        int    rc;

        cfs_waitq_init(&debug_ctlwq);
        rc = tracefile_init();

        if (rc == 0)
                libcfs_register_panic_notifier();

        return rc;
}

int libcfs_debug_cleanup(void)
{
        libcfs_unregister_panic_notifier();
        tracefile_exit();
        return 0;
}

int libcfs_debug_clear_buffer(void)
{
        trace_flush_pages();
        return 0;
}

/* Debug markers, although printed by S_LNET
 * should not be be marked as such. */
#undef DEBUG_SUBSYSTEM
#define DEBUG_SUBSYSTEM S_UNDEFINED
int libcfs_debug_mark_buffer(char *text)
{
        CDEBUG(D_TRACE,"***************************************************\n");
        CDEBUG(D_WARNING, "DEBUG MARKER: %s\n", text);
        CDEBUG(D_TRACE,"***************************************************\n");

        return 0;
}
#undef DEBUG_SUBSYSTEM
#define DEBUG_SUBSYSTEM S_LNET

void libcfs_debug_set_level(unsigned int debug_level)
{
        printk(KERN_WARNING "Lustre: Setting portals debug level to %08x\n",
               debug_level);
        libcfs_debug = debug_level;
}

EXPORT_SYMBOL(libcfs_debug_dumplog);
EXPORT_SYMBOL(libcfs_debug_set_level);


#else /* !__KERNEL__ */
unsigned int libcfs_subsystem_debug = ~0;

unsigned int libcfs_debug = (D_EMERG | D_ERROR | D_WARNING | D_CONSOLE |
                             D_NETERROR | D_HA | D_CONFIG | D_IOCTL |
                             D_DLMTRACE | D_RPCTRACE | D_VFSTRACE);

#include <libcfs/libcfs.h>

#ifdef HAVE_SYS_USER_H
# include <sys/user.h>
#endif

#ifdef HAVE_CATAMOUNT_DATA_H
#include <catamount/data.h>
#include <catamount/lputs.h>

static char source_nid[16];
/* 0 indicates no messages to console, 1 is errors, > 1 is all debug messages */
static int toconsole = 1;
unsigned int libcfs_console_ratelimit = 1;
#else /* !HAVE_CATAMOUNT_DATA_H */
#ifdef HAVE_NETDB_H
#include <sys/utsname.h>
#endif /* HAVE_CATAMOUNT_DATA_H */
struct utsname *tmp_utsname;
static char source_nid[sizeof(tmp_utsname->nodename)];
#endif /* __KERNEL__ */

static int source_pid;
int smp_processor_id = 1;
char debug_file_path[1024];
FILE *debug_file_fd;

int portals_do_debug_dumplog(__unusedx void *arg)
{
        printf("Look in %s\n", debug_file_name);
        return 0;
}


void portals_debug_print(void)
{
        return;
}


void libcfs_debug_dumplog(void)
{
        printf("Look in %s\n", debug_file_name);
        return;
}

int libcfs_debug_init(__unusedx unsigned long bufsize)
{
        char *debug_mask = NULL;
        char *debug_subsys = NULL;
        char *debug_filename;

#ifdef HAVE_CATAMOUNT_DATA_H
        char *debug_console = NULL;
        char *debug_ratelimit = NULL;

        snprintf(source_nid, sizeof(source_nid) - 1, "%u", _my_pnid);
        source_pid = _my_pid;

        debug_console = getenv("LIBLUSTRE_DEBUG_CONSOLE");
        if (debug_console != NULL) {
                toconsole = strtoul(debug_console, NULL, 0);
                CDEBUG(D_INFO, "set liblustre toconsole to %u\n", toconsole);
        }
        debug_ratelimit = getenv("LIBLUSTRE_DEBUG_CONSOLE_RATELIMIT");
        if (debug_ratelimit != NULL) {
                libcfs_console_ratelimit = strtoul(debug_ratelimit, NULL, 0);
                CDEBUG(D_INFO, "set liblustre console ratelimit to %u\n", libcfs_console_ratelimit);
        }
#else
        struct utsname myname;

        if (uname(&myname) == 0)
                strcpy(source_nid, myname.nodename);
        source_pid = getpid();
#endif
        /* debug masks */
        debug_mask = getenv("LIBLUSTRE_DEBUG_MASK");
        if (debug_mask)
                libcfs_debug = (unsigned int) strtol(debug_mask, NULL, 0);

        debug_subsys = getenv("LIBLUSTRE_DEBUG_SUBSYS");
        if (debug_subsys)
                libcfs_subsystem_debug =
                                (unsigned int) strtol(debug_subsys, NULL, 0);

        debug_filename = getenv("LIBLUSTRE_DEBUG_BASE");
        if (debug_filename)
                strncpy(debug_file_path,debug_filename,sizeof(debug_file_path));

        debug_filename = getenv("LIBLUSTRE_DEBUG_FILE");
        if (debug_filename)
                strncpy(debug_file_name,debug_filename,sizeof(debug_file_path));

        if (debug_file_name[0] == '\0' && debug_file_path[0] != '\0')
                snprintf(debug_file_name, sizeof(debug_file_name) - 1,
                         "%s-%s-%lu.log", debug_file_path, source_nid, time(0));

        if (strcmp(debug_file_name, "stdout") == 0 ||
            strcmp(debug_file_name, "-") == 0) {
                debug_file_fd = stdout;
        } else if (strcmp(debug_file_name, "stderr") == 0) {
                debug_file_fd = stderr;
        } else if (debug_file_name[0] != '\0') {
                debug_file_fd = fopen(debug_file_name, "w");
                if (debug_file_fd == NULL)
                        fprintf(stderr, "%s: unable to open '%s': %s\n",
                                source_nid, debug_file_name, strerror(errno));
        }

        if (debug_file_fd == NULL)
                debug_file_fd = stdout;

        return 0;
}

int libcfs_debug_cleanup(void)
{
        if (debug_file_fd != stdout && debug_file_fd != stderr)
                fclose(debug_file_fd);
        return 0;
}

int libcfs_debug_clear_buffer(void)
{
        return 0;
}

int libcfs_debug_mark_buffer(char *text)
{

        fprintf(debug_file_fd, "*******************************************************************************\n");
        fprintf(debug_file_fd, "DEBUG MARKER: %s\n", text);
        fprintf(debug_file_fd, "*******************************************************************************\n");

        return 0;
}

#ifdef HAVE_CATAMOUNT_DATA_H

#define CATAMOUNT_MAXLINE (256-4)
void catamount_printline(char *buf, size_t size)
{
    char *pos = buf;
    int prsize = size;

    while (prsize > 0){
        lputs(pos);
        pos += CATAMOUNT_MAXLINE;
        prsize -= CATAMOUNT_MAXLINE;
    }
}

#endif

int
libcfs_debug_vmsg2(__unusedx cfs_debug_limit_state_t *cdls,
                   __unusedx int subsys, int mask,
                   const char *file, const char *fn, const int line,
                   const char *format1, va_list args,
                   const char *format2, ...)
{
        struct timeval tv;
        int            nob;
        int            remain;
        va_list        ap;
        char           buf[PAGE_SIZE]; /* size 4096 used for compatible with linux,
                                   * where message can`t be exceed PAGE_SIZE */
        int            console = 0;
        char *prefix = "Lustre";

#ifdef HAVE_CATAMOUNT_DATA_H
        /* toconsole == 0 - all messages to debug_file_fd
         * toconsole == 1 - warnings to console, all to debug_file_fd
         * toconsole >  1 - all debug to console */
        if ( ((mask & D_CANTMASK) &&
             (toconsole == 1)) || (toconsole > 1)) {
                console = 1;
        }
#endif

        if ((!console) && (!debug_file_fd)) {
                return 0;
        }

        if (mask & (D_EMERG | D_ERROR))
               prefix = "LustreError";

        nob = snprintf(buf, sizeof(buf), "%s: %u-%s:(%s:%d:%s()): ", prefix,
                       source_pid, source_nid, file, line, fn);

        remain = sizeof(buf) - nob;
        if (format1) {
                nob += vsnprintf(&buf[nob], remain, format1, args);
        }

        remain = sizeof(buf) - nob;
        if ((format2) && (remain > 0)) {
                va_start(ap, format2);
                nob += vsnprintf(&buf[nob], remain, format2, ap);
                va_end(ap);
        }

#ifdef HAVE_CATAMOUNT_DATA_H
        if (console) {
                /* check rate limit for console */
                if (cdls != NULL) {
                        cfs_time_t t = cdls->cdls_next +
                                       cfs_time_seconds(CDEBUG_MAX_LIMIT + 10);
                        cfs_duration_t  dmax = cfs_time_seconds(CDEBUG_MAX_LIMIT);

                        if (libcfs_console_ratelimit &&
                                cdls->cdls_next != 0 &&     /* not first time ever */
                                !cfs_time_after(cfs_time_current(), cdls->cdls_next)) {

                                /* skipping a console message */
                                cdls->cdls_count++;
                                goto out_file;
                        }

                        if (cfs_time_after(cfs_time_current(), t)) {
                                /* last timeout was a long time ago */
                                cdls->cdls_delay /= 8;
                        } else {
                                cdls->cdls_delay *= 2;

                                if (cdls->cdls_delay < CFS_TICK)
                                        cdls->cdls_delay = CFS_TICK;
                                else if (cdls->cdls_delay > dmax)
                                        cdls->cdls_delay = dmax;
                        }

                        /* ensure cdls_next is never zero after it's been seen */
                        cdls->cdls_next = (cfs_time_current() + cdls->cdls_delay) | 1;
                }

                if (cdls != NULL && cdls->cdls_count != 0) {
                        char buf2[100];

                        nob = snprintf(buf2, sizeof(buf2),
                                       "Skipped %d previous similar message%s\n",
                                       cdls->cdls_count, (cdls->cdls_count > 1) ? "s" : "");

                        catamount_printline(buf2, nob);
                        cdls->cdls_count = 0;
                        goto out_file;
                }
                catamount_printline(buf, nob);
       }
out_file:
        /* return on toconsole > 1, as we don't want the user getting
        * spammed by the debug data */
        if (toconsole > 1)
                return 0;
#endif
        if (debug_file_fd == NULL)
                return 0;

        gettimeofday(&tv, NULL);

        fprintf(debug_file_fd, "%lu.%06lu:%u:%s:(%s:%d:%s()): %s",
                tv.tv_sec, tv.tv_usec, source_pid, source_nid,
                file, line, fn, buf);

        return 0;
}

void
libcfs_assertion_failed(const char *expr, const char *file, const char *func,
                        const int line)
{
        libcfs_debug_msg(NULL, 0, D_EMERG, file, func, line,
                         "ASSERTION(%s) failed\n", expr);
        abort();
}

#endif /* __KERNEL__ */
