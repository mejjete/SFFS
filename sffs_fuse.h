#ifndef SFFS_FUSE_H
#define SFFS_FUSE_H
#define FUSE_USE_VERSION 30

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#if defined(SOLARIS)
#define _XOPEN_SOURCE 600
#else 
#define _XOPEN_SOURCE 700
#endif

#include <fuse.h>
#include <fuse_opt.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include "sffs_context.h"

// File system image file
#define SFFS_IMAGE      ".__sffs_image"

/**
 *  DEBUG macro allows for a file system silently define all
 *  available operations to a stub that prints "func not implemented"
 *  to a log file
*/
#ifdef DEBUG
#define THUMB_FUNC  printlg("%s not implemented", __func__);
#else 
#define THUMB_FUNC
#endif

// sffs.c
int sffs_getattr(const char *, struct stat *);

int sffs_creat(const char *, mode_t, dev_t);

int sffs_mkdir(const char *, mode_t);

int sffs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
			struct fuse_file_info *);
    
int sffs_readlink(const char *, char *, size_t);

int sffs_mknod(const char *, mode_t, dev_t);

int sffs_unlink(const char *);

int sffs_rmdir(const char *);

int sffs_symlink(const char *, const char *);

int sffs_rename(const char *, const char *);

int sffs_link(const char *, const char *);

int sffs_chmod(const char *, mode_t);

int sffs_chown(const char *, uid_t, gid_t);

int sffs_truncate(const char *, off_t);

int sffs_open(const char *, struct fuse_file_info *);

int sffs_read(const char *, char *, size_t, off_t,
		     struct fuse_file_info *);

int sffs_write(const char *, const char *, size_t, off_t,
		      struct fuse_file_info *);

int sffs_statfs(const char *, struct statvfs *);

int sffs_flush(const char *, struct fuse_file_info *);

int sffs_release(const char *, struct fuse_file_info *);

int sffs_fsync(const char *, int, struct fuse_file_info *);

int sffs_setxattr(const char *, const char *, const char *, size_t, int);

int sffs_getxattr(const char *, const char *, char *, size_t);

int sffs_listxattr(const char *, char *, size_t);

int sffs_removexattr(const char *, const char *);

int sffs_opendir(const char *, struct fuse_file_info *);

int sffs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
        struct fuse_file_info *);

int sffs_releasedir(const char *, struct fuse_file_info *);

int sffs_fsyncdir(const char *, int, struct fuse_file_info *);

void *sffs_init(struct fuse_conn_info *conn);

void sffs_destroy(void *);

int sffs_access(const char *, int);

int sffs_create(const char *, mode_t, struct fuse_file_info *);

int sffs_ftruncate(const char *, off_t, struct fuse_file_info *);

int sffs_fgetattr(const char *, struct stat *, struct fuse_file_info *);

int sffs_lock(const char *, struct fuse_file_info *, int cmd,
            struct flock *);

int sffs_utimens(const char *, const struct timespec tv[2]);

int sffs_bmap(const char *, size_t blocksize, uint64_t *idx);

int sffs_ioctl(const char *, int cmd, void *arg,
            struct fuse_file_info *, unsigned int flags, void *data);

int sffs_poll(const char *, struct fuse_file_info *,
            struct fuse_pollhandle *ph, unsigned *reventsp);

int sffs_write_buf(const char *, struct fuse_bufvec *buf, off_t off,
            struct fuse_file_info *);

int sffs_read_buf(const char *, struct fuse_bufvec **bufp,
            size_t size, off_t off, struct fuse_file_info *);

int sffs_flock(const char *, struct fuse_file_info *, int op);

int sffs_fallocate(const char *, int, off_t, off_t,
            struct fuse_file_info *);

#endif  // SFFS_FUSE_H