/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#include <sffs_fuse.h>
#include <sffs_err.h>
#include <sffs.h>
#include <sffs_device.h>

void *sffs_init(struct fuse_conn_info *conn)
{   
    /**
     *  SFFS image is created my mkfs.sffs. This handler only 
     *  responsible for memory allocation and both superblock 
     *  and file system context initialization
    */
    struct sffs_context *sffs_context = (struct sffs_context *)
            malloc(sizeof(struct sffs_context));
    if(!sffs_context)
        abort();

    sffs_err_t errc = sffs_read_sb(sffs_context, &sffs_context->sb);
    if(errc < 0)
        abort();

    // Allocate at least block_size cache for local use
    void *cache = malloc(sffs_context->sb.s_block_size);
    if(!cache)
        abort();
    
    sffs_context->cache = cache;
    return sffs_context;
}

void sffs_destroy(void *data)
{
    sffs_context_t *ctx = (sffs_context_t *) fuse_get_context();
    if(sffs_write_sb(ctx, &ctx->sb) < 0)
        ; // do high level error handling

    close(ctx->disk_id);
    close(ctx->log_id);
}

int sffs_statfs(const char *path, struct statvfs *statfs)
{
    sffs_context_t *ctx = (sffs_context_t *) fuse_get_context();
    struct sffs_superblock *sb = &ctx->sb;

    statfs->f_bsize = sb->s_block_size;
    statfs->f_blocks = sb->s_blocks_count;
    statfs->f_bfree = sb->s_free_blocks_count;
    statfs->f_files = sb->s_inodes_count;
    statfs->f_ffree = sb->s_free_inodes_count;
    statfs->f_fsid = sb->s_magic;

    // Update superblock on disk
    sffs_write_sb(0, sb);
    return 0;
}

int sffs_getattr(const char *path, struct stat *st)
{
    int res = -1;

    st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = time(NULL);
	st->st_mtime = time(NULL);

    if(strcmp(path, "/" ) == 0 )
	{
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 3;
        res = 0;
	}
    else 
    {
        st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = 1024;
        res = 0;
    }

    return res;
};


int sffs_mkdir(const char *path, mode_t mode) 
{ return 0; }


int sffs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
			struct fuse_file_info *fi)
{
    return 0;
}

int sffs_read(const char *, char *, size_t, off_t, struct fuse_file_info *)
{
    return 0;
}

#ifdef SFFS_THUMB

int sffs_readlink(const char *, char *, size_t) { THUMB_FUNC; }

int sffs_mknod(const char *, mode_t, dev_t) { THUMB_FUNC; }

int sffs_unlink(const char *) { THUMB_FUNC; }

int sffs_rmdir (const char *) { THUMB_FUNC; }

int sffs_symlink(const char *, const char *) { THUMB_FUNC; }

int sffs_rename(const char *, const char *) { THUMB_FUNC; }

int sffs_link(const char *, const char *) { THUMB_FUNC; }

int sffs_chmod(const char *, mode_t) { THUMB_FUNC; }

int sffs_chown(const char *, uid_t, gid_t) { THUMB_FUNC; }

int sffs_truncate(const char *, off_t) { THUMB_FUNC; }

int sffs_open(const char *, struct fuse_file_info *) { THUMB_FUNC; }

int sffs_read(const char *, char *, size_t, off_t,
		     struct fuse_file_info *) { THUMB_FUNC; }

int sffs_write(const char *, const char *, size_t, off_t,
		      struct fuse_file_info *) { THUMB_FUNC; }

int sffs_statfs(const char *, struct statvfs *) { THUMB_FUNC; }

int sffs_flush(const char *, struct fuse_file_info *) { THUMB_FUNC; }


int sffs_release(const char *, struct fuse_file_info *) { THUMB_FUNC; }

int sffs_fsync(const char *, int, struct fuse_file_info *) { THUMB_FUNC; }


int sffs_setxattr(const char *, const char *, const char *, size_t, int) { THUMB_FUNC; }

int sffs_getxattr(const char *, const char *, char *, size_t) { THUMB_FUNC; }

int sffs_listxattr(const char *, char *, size_t) { THUMB_FUNC; }

int sffs_removexattr(const char *, const char *) { THUMB_FUNC; }

int sffs_opendir(const char *, struct fuse_file_info *) { THUMB_FUNC; }

int sffs_releasedir(const char *, struct fuse_file_info *) { THUMB_FUNC; }

int sffs_fsyncdir(const char *, int, struct fuse_file_info *) { THUMB_FUNC; }

void *sffs_destroy(void *) { THUMB_FUNC; }

int sffs_access(const char *, int) { THUMB_FUNC; }

int sffs_create(const char *, mode_t, struct fuse_file_info *) { THUMB_FUNC; }

int sffs_ftruncate(const char *, off_t, struct fuse_file_info *) { THUMB_FUNC; }

int sffs_fgetattr(const char *, struct stat *, struct fuse_file_info *) { THUMB_FUNC; }

int sffs_lock(const char *, struct fuse_file_info *, int cmd,
            struct flock *) { THUMB_FUNC; }

int sffs_utimens(const char *, const struct timespec tv[2]) { THUMB_FUNC; }

int sffs_bmap(const char *, size_t blocksize, uint64_t *idx) { THUMB_FUNC; }

int sffs_ioctl(const char *, int cmd, void *arg,
            struct fuse_file_info *, unsigned int flags, void *data) { THUMB_FUNC; }

int sffs_poll(const char *, struct fuse_file_info *,
            struct fuse_pollhandle *ph, unsigned *reventsp) { THUMB_FUNC; }

int sffs_write_buf(const char *, struct fuse_bufvec *buf, off_t off,
            struct fuse_file_info *) { THUMB_FUNC; }

int sffs_read_buf(const char *, struct fuse_bufvec **bufp,
            size_t size, off_t off, struct fuse_file_info *) { THUMB_FUNC; }

int sffs_flock(const char *, struct fuse_file_info *, int op) { THUMB_FUNC; }

int sffs_fallocate(const char *, int, off_t, off_t,
            struct fuse_file_info *) { THUMB_FUNC; }

#endif // SFFS_THUMB
