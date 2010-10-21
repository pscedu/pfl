/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2010, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#ifndef _PFL_FS_H_
#define _PFL_FS_H_

#include <sys/types.h>

#include <stdint.h>

struct stat;
struct statvfs;

struct pscfs_args;
struct pscfs_req;

typedef uint64_t pscfs_inum_t;
typedef uint64_t pscfs_fgen_t;

struct pscfs_cred {
	uint32_t	pfc_uid;
	uint32_t	pfc_gid;
};

struct pscfs_dirent {
	uint64_t	pfd_ino;
	uint64_t	pfd_off;
	uint32_t	pfd_namelen;
	uint32_t	pfd_type;
	char		pfd_name[0];
};

#define PFL_DIRENT_NAME_OFFSET		offsetof(struct pscfs_dirent, pfd_name)
#define PFL_DIRENT_ALIGN(x)		(((x) + sizeof(uint64_t) - 1) &	\
					    ~(sizeof(uint64_t) - 1))
#define PFL_DIRENT_SIZE(namelen)	PFL_DIRENT_ALIGN(		\
					    PFL_DIRENT_NAME_OFFSET + (namelen))

/* user fills these in */
struct pscfs {
	void	(*pf_handle_access)(struct pscfs_req *, pscfs_inum_t, int);
	void	(*pf_handle_close)(struct pscfs_req *, void *);
	void	(*pf_handle_closedir)(struct pscfs_req *, void *);
	void	(*pf_handle_create)(struct pscfs_req *, pscfs_inum_t, const char *, int, mode_t);
	void	(*pf_handle_flush)(struct pscfs_req *, void *);
	void	(*pf_handle_fsync)(struct pscfs_req *, int, void *);
	void	(*pf_handle_fsyncdir)(struct pscfs_req *, int, void *);
	void	(*pf_handle_getattr)(struct pscfs_req *, pscfs_inum_t);
	void	(*pf_handle_ioctl)(struct pscfs_req *);
	void	(*pf_handle_link)(struct pscfs_req *, pscfs_inum_t, pscfs_inum_t, const char *);
	void	(*pf_handle_lookup)(struct pscfs_req *, pscfs_inum_t, const char *);
	void	(*pf_handle_mkdir)(struct pscfs_req *, pscfs_inum_t, const char *, mode_t);
	void	(*pf_handle_mknod)(struct pscfs_req *, pscfs_inum_t, const char *, mode_t, dev_t);
	void	(*pf_handle_open)(struct pscfs_req *, pscfs_inum_t, int);
	void	(*pf_handle_opendir)(struct pscfs_req *, pscfs_inum_t, int);
	void	(*pf_handle_read)(struct pscfs_req *, size_t, off_t, void *);
	void	(*pf_handle_readdir)(struct pscfs_req *, size_t, off_t, void *);
	void	(*pf_handle_readlink)(struct pscfs_req *, pscfs_inum_t);
	void	(*pf_handle_rename)(struct pscfs_req *, pscfs_inum_t, const char *, pscfs_inum_t, const char *);
	void	(*pf_handle_rmdir)(struct pscfs_req *, pscfs_inum_t, const char *);
	void	(*pf_handle_setattr)(struct pscfs_req *, pscfs_inum_t, struct stat *, int, void *);
	void	(*pf_handle_statfs)(struct pscfs_req *);
	void	(*pf_handle_symlink)(struct pscfs_req *, const char *, pscfs_inum_t, const char *);
	void	(*pf_handle_unlink)(struct pscfs_req *, pscfs_inum_t, const char *);
	void	(*pf_handle_umount)();
	void	(*pf_handle_write)(struct pscfs_req *, const void *, size_t, off_t, void *);
};

void	pscfs_addarg(struct pscfs_args *, const char *);
void	pscfs_freeargs(struct pscfs_args *);
void	pscfs_mount(const char *, struct pscfs_args *);

void	pscfs_getcreds(struct pscfs_req *, struct pscfs_cred *);
mode_t	pscfs_getumask(struct pscfs_req *);

int	pscfs_setdebug(int);
int	pscfs_getdebug(int *);

int	pscfs_main(void);
void	pscfs_mount(const char *, struct pscfs_args *);

void	pscfs_reply_access(struct pscfs_req *, int);
void	pscfs_reply_close(struct pscfs_req *, int);
void	pscfs_reply_closedir(struct pscfs_req *, int);
void	pscfs_reply_create(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, double, const struct stat *, double, void *, int, int);
void	pscfs_reply_flush(struct pscfs_req *, int);
void	pscfs_reply_fsync(struct pscfs_req *, int);
void	pscfs_reply_fsyncdir(struct pscfs_req *, int);
void	pscfs_reply_getattr(struct pscfs_req *, struct stat *, double, int);
void	pscfs_reply_init(struct pscfs_req *);
void	pscfs_reply_ioctl(struct pscfs_req *);
void	pscfs_reply_link(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, const struct stat *, int, int);
void	pscfs_reply_lookup(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, const struct stat *, int, int);
void	pscfs_reply_mkdir(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, const struct stat *, int, int);
void	pscfs_reply_mknod(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, double, const struct stat *, double, int);
void	pscfs_reply_open(struct pscfs_req *, void *, int, int);
void	pscfs_reply_opendir(struct pscfs_req *, void *, int, int);
void	pscfs_reply_read(struct pscfs_req *, void *, ssize_t, int);
void	pscfs_reply_readdir(struct pscfs_req *, void *, ssize_t, int);
void	pscfs_reply_readlink(struct pscfs_req *, void *, int);
void	pscfs_reply_rename(struct pscfs_req *, int);
void	pscfs_reply_rmdir(struct pscfs_req *, int);
void	pscfs_reply_setattr(struct pscfs_req *, struct stat *, double, int);
void	pscfs_reply_statfs(struct pscfs_req *, struct statvfs *, int);
void	pscfs_reply_symlink(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, const struct stat *, int, int);
void	pscfs_reply_umount(struct pscfs_req *);
void	pscfs_reply_unlink(struct pscfs_req *, int);
void	pscfs_reply_write(struct pscfs_req *, ssize_t, int);

#define PSCFS_CREATEF_DIO	(1 << 0)

#define PSCFS_OPENF_DIO		(1 << 0)
#define PSCFS_OPENF_KEEPCACHE	(1 << 1)

/* setattr() to_set mask flags */
#define PSCFS_SETATTRF_MODE	(1 << 0)	/* chmod */
#define	PSCFS_SETATTRF_UID	(1 << 1)	/* chown */
#define	PSCFS_SETATTRF_GID	(1 << 2)	/* chgrp */
#define	PSCFS_SETATTRF_DATASIZE	(1 << 3)	/* file data truncate */
#define	PSCFS_SETATTRF_ATIME	(1 << 4)	/* utimes */
#define	PSCFS_SETATTRF_MTIME	(1 << 5)	/* utimes */
#define	PSCFS_SETATTRF_CTIME	(1 << 6)	/* utimes */
#define	_PSCFS_SETATTRF_LAST	(1 << 7)

#define PSCFS_SETATTRF_ALL	(~0)

extern struct pscfs pscfs;

#endif /* _PFL_FS_H_ */
