/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *   This file is part of Lustre, http://www.lustre.org
 *
 * Lustre public user-space interface definitions.
 */

#ifndef _LUSTRE_USER_H
#define _LUSTRE_USER_H

#ifdef HAVE_ASM_TYPES_H
#include <asm/types.h>
#else
#include <lustre/types.h>
#endif

#ifdef HAVE_QUOTA_SUPPORT
#ifdef __KERNEL__
# include <linux/quota.h>
#else
# include <sys/quota.h>
#endif
#endif

/*
 * asm-x86_64/processor.h on some SLES 9 distros seems to use
 * kernel-only typedefs.  fortunately skipping it altogether is ok
 * (for now).
 */
#define __ASM_X86_64_PROCESSOR_H

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#include <sys/stat.h>
#endif

/* for statfs() */
#define LL_SUPER_MAGIC 0x0BD00BD0

#ifndef EXT3_IOC_GETFLAGS
#define EXT3_IOC_GETFLAGS               _IOR('f', 1, long)
#define EXT3_IOC_SETFLAGS               _IOW('f', 2, long)
#define EXT3_IOC_GETVERSION             _IOR('f', 3, long)
#define EXT3_IOC_SETVERSION             _IOW('f', 4, long)
#define EXT3_IOC_GETVERSION_OLD         _IOR('v', 1, long)
#define EXT3_IOC_SETVERSION_OLD         _IOW('v', 2, long)
#endif

struct obd_statfs;

/* 
 * The ioctl naming rules:
 * LL_*     - works on the currently opened filehandle instead of parent dir
 * *_OBD_*  - gets data for both OSC or MDC (LOV, LMV indirectly)
 * *_MDC_*  - gets/sets data related to MDC
 * *_LOV_*  - gets/sets data related to OSC/LOV
 * *FILE*   - called on parent dir and passes in a filename
 * *STRIPE* - set/get lov_user_md
 * *INFO    - set/get lov_user_mds_data
 */
#define LL_IOC_GETFLAGS                 _IOR ('f', 151, long)
#define LL_IOC_SETFLAGS                 _IOW ('f', 152, long)
#define LL_IOC_CLRFLAGS                 _IOW ('f', 153, long)
#define LL_IOC_LOV_SETSTRIPE            _IOW ('f', 154, long)
#define LL_IOC_LOV_GETSTRIPE            _IOW ('f', 155, long)
#define LL_IOC_LOV_SETEA                _IOW ('f', 156, long)
#define LL_IOC_RECREATE_OBJ             _IOW ('f', 157, long)
#define LL_IOC_GROUP_LOCK               _IOW ('f', 158, long)
#define LL_IOC_GROUP_UNLOCK             _IOW ('f', 159, long)
#define LL_IOC_QUOTACHECK               _IOW ('f', 160, int)
#define LL_IOC_POLL_QUOTACHECK          _IOR ('f', 161, struct if_quotacheck *)
#define LL_IOC_QUOTACTL                 _IOWR('f', 162, struct if_quotactl *)
#define LL_IOC_JOIN                     _IOW ('f', 163, long)
#define IOC_OBD_STATFS                  _IOWR('f', 164, struct obd_statfs *)
#define IOC_LOV_GETINFO                 _IOWR('f', 165, struct lov_user_mds_data *)

#define LL_STATFS_MDC           1
#define LL_STATFS_LOV           2

#define IOC_MDC_TYPE            'i'
#define IOC_MDC_LOOKUP          _IOWR(IOC_MDC_TYPE, 20, struct obd_device *)
#define IOC_MDC_GETFILESTRIPE   _IOWR(IOC_MDC_TYPE, 21, struct lov_user_md *)
#define IOC_MDC_GETFILEINFO     _IOWR(IOC_MDC_TYPE, 22, struct lov_user_mds_data *)
#define LL_IOC_MDC_GETINFO      _IOWR(IOC_MDC_TYPE, 23, struct lov_user_mds_data *)

/* Keep these for backward compartability. */
#define LL_IOC_OBD_STATFS       IOC_OBD_STATFS
#define IOC_MDC_GETSTRIPE       IOC_MDC_GETFILESTRIPE

#define O_LOV_DELAY_CREATE 0100000000  /* hopefully this does not conflict */
#define O_JOIN_FILE        0400000000  /* hopefully this does not conflict */

#define LL_FILE_IGNORE_LOCK             0x00000001
#define LL_FILE_GROUP_LOCKED            0x00000002
#define LL_FILE_READAHEAD               0x00000004

#define LOV_USER_MAGIC_V1 0x0BD10BD0
#define LOV_USER_MAGIC    LOV_USER_MAGIC_V1

#define LOV_USER_MAGIC_JOIN 0x0BD20BD0

#define LOV_PATTERN_RAID0 0x001
#define LOV_PATTERN_RAID1 0x002
#define LOV_PATTERN_FIRST 0x100

#define lov_user_ost_data lov_user_ost_data_v1
struct lov_user_ost_data_v1 {     /* per-stripe data structure */
        __u64 l_object_id;        /* OST object ID */
        __u64 l_object_gr;        /* OST object group (creating MDS number) */
        __u32 l_ost_gen;          /* generation of this OST index */
        __u32 l_ost_idx;          /* OST index in LOV */
} __attribute__((packed));

#define lov_user_md lov_user_md_v1
struct lov_user_md_v1 {           /* LOV EA user data (host-endian) */
        __u32 lmm_magic;          /* magic number = LOV_USER_MAGIC_V1 */
        __u32 lmm_pattern;        /* LOV_PATTERN_RAID0, LOV_PATTERN_RAID1 */
        __u64 lmm_object_id;      /* LOV object ID */
        __u64 lmm_object_gr;      /* LOV object group */
        __u32 lmm_stripe_size;    /* size of stripe in bytes */
        __u16 lmm_stripe_count;   /* num stripes in use for this object */
        __u16 lmm_stripe_offset;  /* starting stripe offset in lmm_objects */
        struct lov_user_ost_data_v1 lmm_objects[0]; /* per-stripe data */
} __attribute__((packed));

#if defined(__x86_64__) || defined(__ia64__) || defined(__ppc64__) || \
    defined(__craynv)
typedef struct stat     lstat_t;
#define lstat_f         lstat
#define HAVE_LOV_USER_MDS_DATA
#elif defined(__USE_LARGEFILE64) || defined(__KERNEL__)
typedef struct stat64   lstat_t;
#define lstat_f         lstat64
#define HAVE_LOV_USER_MDS_DATA
#endif

/* Compile with -D_LARGEFILE64_SOURCE or -D_GNU_SOURCE (or #define) to
 * use this.  It is unsafe to #define those values in this header as it
 * is possible the application has already #included <sys/stat.h>. */
#ifdef HAVE_LOV_USER_MDS_DATA
#define lov_user_mds_data lov_user_mds_data_v1
struct lov_user_mds_data_v1 {
        lstat_t lmd_st;                 /* MDS stat struct */
        struct lov_user_md_v1 lmd_lmm;  /* LOV EA user data */
} __attribute__((packed));
#endif

struct lov_user_ost_data_join {   /* per-stripe data structure */
        __u64 l_extent_start;     /* extent start*/
        __u64 l_extent_end;       /* extent end*/
        __u64 l_object_id;        /* OST object ID */
        __u64 l_object_gr;        /* OST object group (creating MDS number) */
        __u32 l_ost_gen;          /* generation of this OST index */
        __u32 l_ost_idx;          /* OST index in LOV */
} __attribute__((packed));

/* Identifier for a single log object */
struct llog_logid {
        __u64                   lgl_oid;
        __u64                   lgl_ogr;
        __u32                   lgl_ogen;
} __attribute__((packed));

struct lov_user_md_join {         /* LOV EA user data (host-endian) */
        __u32 lmm_magic;          /* magic number = LOV_MAGIC_JOIN */
        __u32 lmm_pattern;        /* LOV_PATTERN_RAID0, LOV_PATTERN_RAID1 */
        __u64 lmm_object_id;      /* LOV object ID */
        __u64 lmm_object_gr;      /* LOV object group */
        __u32 lmm_stripe_size;    /* size of stripe in bytes */
        __u32 lmm_stripe_count;   /* num stripes in use for this object */
        __u32 lmm_extent_count;   /* extent count of lmm*/
        __u64 lmm_tree_id;        /* mds tree object id */
        __u64 lmm_tree_gen;       /* mds tree object gen */
        struct llog_logid lmm_array_id; /* mds extent desc llog object id */
        struct lov_user_ost_data_join lmm_objects[0]; /* per-stripe data */
} __attribute__((packed));


struct ll_recreate_obj {
        __u64 lrc_id;
        __u32 lrc_ost_idx;
};

struct obd_uuid {
        __u8 uuid[40];
};

static inline int obd_uuid_equals(struct obd_uuid *u1, struct obd_uuid *u2)
{
        return strcmp((char *)u1->uuid, (char *)u2->uuid) == 0;
}

static inline int obd_uuid_empty(struct obd_uuid *uuid)
{
        return uuid->uuid[0] == '\0';
}

static inline void obd_str2uuid(struct obd_uuid *uuid, char *tmp)
{
        strncpy((char *)uuid->uuid, tmp, sizeof(*uuid));
        uuid->uuid[sizeof(*uuid) - 1] = '\0';
}

#define LUSTRE_Q_QUOTAON  0x800002     /* turn quotas on */
#define LUSTRE_Q_QUOTAOFF 0x800003     /* turn quotas off */
#define LUSTRE_Q_GETINFO  0x800005     /* get information about quota files */
#define LUSTRE_Q_SETINFO  0x800006     /* set information about quota files */
#define LUSTRE_Q_GETQUOTA 0x800007     /* get user quota structure */
#define LUSTRE_Q_SETQUOTA 0x800008     /* set user quota structure */

#define UGQUOTA 2       /* set both USRQUOTA and GRPQUOTA */

#define QFMT_LDISKFS 2  /* QFMT_VFS_V0(2), quota format for ldiskfs */

struct if_quotacheck {
        __u8                    obd_type[16];
        struct obd_uuid         obd_uuid;
};

#define MDS_GRP_DOWNCALL_MAGIC 0x6d6dd620

struct mds_grp_downcall_data {
        __u32           mgd_magic;
        __u32           mgd_err;
        __u32           mgd_uid;
        __u32           mgd_gid;
        __u32           mgd_ngroups;
        __u32           mgd_groups[0];
};


#ifndef __KERNEL__
#define NEED_QUOTA_DEFS
#else
# include <linux/version.h>
# if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,21)
#  define NEED_QUOTA_DEFS
# endif
#endif

#ifdef NEED_QUOTA_DEFS
#ifndef QUOTABLOCK_BITS
#define QUOTABLOCK_BITS 10
#endif

#ifndef QUOTABLOCK_SIZE
#define QUOTABLOCK_SIZE (1 << QUOTABLOCK_BITS)
#endif

#ifndef toqb
#define toqb(x) (((x) + QUOTABLOCK_SIZE - 1) >> QUOTABLOCK_BITS)
#endif

#ifndef QIF_BLIMITS
#define QIF_BLIMITS     1
#define QIF_SPACE       2
#define QIF_ILIMITS     4
#define QIF_INODES      8
#define QIF_BTIME       16
#define QIF_ITIME       32
#define QIF_LIMITS      (QIF_BLIMITS | QIF_ILIMITS)
#define QIF_USAGE       (QIF_SPACE | QIF_INODES)
#define QIF_TIMES       (QIF_BTIME | QIF_ITIME)
#define QIF_ALL         (QIF_LIMITS | QIF_USAGE | QIF_TIMES)
#endif

#endif /* !__KERNEL__ */

/* XXX: same as if_dqinfo struct in kernel */
struct obd_dqinfo {
        __u64 dqi_bgrace;
        __u64 dqi_igrace;
        __u32 dqi_flags;
        __u32 dqi_valid;
};

/* XXX: same as if_dqblk struct in kernel, plus one padding */
struct obd_dqblk {
        __u64 dqb_bhardlimit;
        __u64 dqb_bsoftlimit;
        __u64 dqb_curspace;
        __u64 dqb_ihardlimit;
        __u64 dqb_isoftlimit;
        __u64 dqb_curinodes;
        __u64 dqb_btime;
        __u64 dqb_itime;
        __u32 dqb_valid;
        __u32 padding;
};

struct if_quotactl {
        __u32                   qc_cmd;
        __u32                   qc_type;
        __u32                   qc_id;
        __u32                   qc_stat;
        struct obd_dqinfo       qc_dqinfo;
        struct obd_dqblk        qc_dqblk;
        __u8                    obd_type[16];
        struct obd_uuid         obd_uuid;
};

#ifndef LPU64
#if !defined(BITS_PER_LONG) && defined(__WORDSIZE)
#define BITS_PER_LONG __WORDSIZE
#endif
/* x86_64 defines __u64 as "long" in userspace, but "long long" in the kernel */
#if defined(__x86_64__) && defined(__KERNEL__)
# define LPU64 "%Lu"
# define LPD64 "%Ld"
# define LPX64 "%#Lx"
# define LPSZ  "%lu"
# define LPSSZ "%ld"
#elif (BITS_PER_LONG == 32 || __WORDSIZE == 32)
# define LPU64 "%Lu"
# define LPD64 "%Ld"
# define LPX64 "%#Lx"
# define LPSZ  "%u"
# define LPSSZ "%d"
#elif (BITS_PER_LONG == 64 || __WORDSIZE == 64)
# define LPU64 "%lu"
# define LPD64 "%ld"
# define LPX64 "%#lx"
# define LPSZ  "%lu"
# define LPSSZ "%ld"
#endif
#endif /* !LPU64 */

#ifndef offsetof
# define offsetof(typ,memb)     ((unsigned long)((char *)&(((typ *)0)->memb)))
#endif

#endif /* _LUSTRE_USER_H */
