#include "sffs_fuse.h"
#include "sffs_err.h"
#include "sffs.h"
#include "sffs_device.h"

void *sffs_init(struct fuse_conn_info *conn)
{   
    printf("SFFS_INIT\n");

    // Initialize image file
    int flags = O_RDWR;
    mode_t fmode = 0;
    struct stat statbuf; 

    if(stat(SFFS_IMAGE, &statbuf) == -1)
    {
        flags |= O_CREAT | O_TRUNC;
        fmode = S_IRWXU | S_IRWXG | S_IRWXO;
    }

    int fd;
    if((fd = open(SFFS_IMAGE, flags, fmode)) < 0)
        err_sys("Cannot initialize sffs image");

    sffs_ctx.disk_id = fd;
    if((flags & O_CREAT) == O_CREAT)
    {
        if(ftruncate(fd, 52428800) < 0)
            err_sys("Cannot create sffs image with specified size");
        
        __sffs_init();
        return conn;
    }

    if(lseek64(sffs_ctx.disk_id, 1024, SEEK_SET) < 0)
        err_sys("sffs: Seek error\n");
    
    if(read(sffs_ctx.disk_id, &sffs_ctx.sb, SFFS_SB_SIZE) < 0)
        err_sys("sffs: Read error\n");

    // do not know what to return so return fuse's connector
    return conn;
}

void sffs_destroy(void *data)
{
    printf("SFFS_DESTROY\n");
    close(sffs_ctx.disk_id);
    close(sffs_ctx.log_id);
}

int sffs_statfs(const char *path, struct statvfs *statfs)
{
    struct sffs_superblock sb;
    sffs_read_sb(1, &sb);

    statfs->f_bsize = sb.s_block_size;
    statfs->f_blocks = sb.s_blocks_count;
    statfs->f_bfree = sb.s_free_blocks_count;
    statfs->f_files = sb.s_inodes_count;
    statfs->f_ffree = sb.s_free_inodes_count;
    statfs->f_fsid = sb.s_magic;

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
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    static struct stat statbuf;
    statbuf.st_ino = 12345;

    if(strcmp(path, "/") == 0)
	{
		filler(buf, "file54", &statbuf, 0);
		filler(buf, "file349", NULL, 0);
	}

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
