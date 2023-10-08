/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#include <sffs_fuse.h>
#include <sffs_err.h>
#include <sffs.h>
#include <sffs_device.h>
#include <errno.h>


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
    
    struct sffs_options *opts = (struct sffs_options *) __sffs_pd;
    if(!opts)
        abort();

    // Obtain pre-init parameter via global variable
    int fd = open(opts->fs_image, O_RDWR);
    if(fd < 0)
        abort();
    sffs_context->disk_id = fd;

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
    struct fuse_context *fctx = fuse_get_context();
    sffs_context_t *ctx = (sffs_context_t *) fctx->private_data;

    if(sffs_write_sb(ctx, &ctx->sb) < 0)
        ; // do high level error handling

    close(ctx->disk_id);
    close(ctx->log_id);
}

int sffs_statfs(const char *path, struct statvfs *statfs)
{
    struct fuse_context *fctx = fuse_get_context();
    sffs_context_t *ctx = (sffs_context_t *) fctx->private_data;
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
    printf("%s\n", path);

    if(strcmp(path, "/") == 0)
    {
        st->st_ino = 0;
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
    }
    else 
    {
        st->st_ino = 55;
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 1;
    }

    return 0;

#if 0
    struct fuse_context *fctx = fuse_get_context();
    sffs_context_t *ctx = (sffs_context_t *) fctx->private_data;

    // Example implementation
    printf("%s\n", path);
    int res = 0;

    memset(st, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) 
    {
        sffs_err_t errc;
        struct sffs_inode_mem *ino_mem;
        errc = sffs_creat_inode(ctx, 0, SFFS_IFREG, 0, &ino_mem);
        if(errc < 0)
            return errc;
        
        errc = sffs_read_inode(ctx, 0, ino_mem);
        if(errc < 0)
            return errc;
        
        st->st_dev = ctx->disk_id;

        // Adding 1 because fuse treats 0 as optional value
        st->st_ino = ino_mem->ino.i_inode_num + 1;

        st->st_mode = ino_mem->ino.i_mode;
        st->st_nlink = ino_mem->ino.i_link_count;
        st->st_uid = ino_mem->ino.i_uid_owner;
        st->st_gid = ino_mem->ino.i_gid_owner;
        st->st_size = ino_mem->ino.i_blks_count;
        st->st_blksize = 1024;
        st->st_blocks = st->st_blksize / 512;

        st->st_atime = 0;
        st->st_mtime = 0;
        st->st_ctime = 0;

        res = 0;
        free(ino_mem);
    } 
    else if (strcmp(path, "/hello.txt") == 0) 
    {
        st->st_mode = S_IFREG | 0644;
        st->st_nlink = 1;
        st->st_size = strlen("Hello, World!");
        res = 0;
    } 
    else 
    {
        res = -ENOENT;
    }

    return res;

#else

    struct fuse_context *fctx = fuse_get_context();
    sffs_context_t *ctx = (sffs_context_t *) fctx->private_data;

    // Example implementation
    printf("%s\n", path);
    memset(st, 0, sizeof(struct stat));
    
    sffs_err_t errc;
    struct sffs_inode_mem *ino_mem;
    errc = sffs_creat_inode(ctx, 0, SFFS_IFREG, 0, &ino_mem);
    if(errc < 0)
        return errc;

    bool exist = false;

    if(strcmp(path, "/") == 0) 
    {
        errc = sffs_read_inode(ctx, 0, ino_mem);
        if(errc < 0)
            return errc;
        exist = true;
    } 
    else 
    {
        struct sffs_inode_mem *child;
        errc = sffs_creat_inode(ctx, 0, SFFS_IFREG, 0, &child);
        if(errc < 0)
            return errc;

        const char *p = path + 1;
        struct sffs_direntry *direntry;
        
        errc = sffs_lookup_direntry(ctx, child, p, &direntry, NULL);
        if(errc == false)
            exist = false;
        else if(errc < 0)
            return -1;

        errc = sffs_read_inode(ctx, direntry->ino_id, child);
        if(errc < 0)
            return -1;
        
        memcpy(ino_mem, child, SFFS_INODE_SIZE + SFFS_INODE_DATA_SIZE);
        exist = true;
        free(child);
    }

    if(!exist)
        return -1;
    
    st->st_dev = ctx->disk_id;

    // Adding 1 because fuse treats 0 as optional value
    st->st_ino = ino_mem->ino.i_inode_num + 1;

    st->st_mode = ino_mem->ino.i_mode;
    st->st_nlink = ino_mem->ino.i_link_count;
    st->st_uid = ino_mem->ino.i_uid_owner;
    st->st_gid = ino_mem->ino.i_gid_owner;
    st->st_size = ino_mem->ino.i_blks_count;
    st->st_blksize = ctx->sb.s_block_size;
    st->st_blocks = st->st_blksize / 512;

    st->st_atime = 0;
    st->st_mtime = 0;
    st->st_ctime = 0;

    free(ino_mem);
    return 0;
#endif
};


int sffs_mkdir(const char *path, mode_t mode) 
{ 
    // struct fuse_context *fctx = fuse_get_context();
    // sffs_context_t *ctx = (sffs_context_t *) fctx->private_data;

    // struct sffs_inode_mem *ino_mem;
    // sffs_err_t errc = sffs_creat_inode(ctx, 0, SFFS_IFDIR, 0, &ino_mem);
    // if(errc < 0)
    //     return -1;

    // // For now, create new directories only in a root
    // errc = sffs_read_inode(ctx, 0, ino_mem);
    // if(errc < 0)
    //     return -1;

    // // Allocate new inode
    // ino32_t new_dir_ino;
    // mode_t new_dir_mode = SFFS_IFDIR | SFFS_IRWXU | SFFS_IRGRP | SFFS_IXGRP
    //     | SFFS_IROTH | SFFS_IXOTH;

    // errc = sffs_alloc_inode(ctx, &new_dir_ino, new_dir_mode);
    // if(errc < 0)
    //     return -1;

    // struct sffs_inode_mem *new_dir;
    // errc = sffs_creat_inode(ctx, new_dir_ino, new_dir_mode, 0, &new_dir);
    // if(errc < 0)
    //     return -1;

    // // Remove leading slash
    // const char *new_dir_name = path + 1;
    // size_t new_dir_len = strlen(new_dir_name) + SFFS_DIRENTRY_LENGTH;
    // struct sffs_direntry *new_direntry = malloc(new_dir_len);
    // if(!new_dir)
    //     return -1;

    // new_direntry->ino_id = new_dir->ino.i_inode_num;
    // new_direntry->file_type = SFFS_DIRENTRY_MODE(new_dir->ino.i_mode);
    // new_direntry->rec_len = new_dir_len;
    // memcpy(new_direntry->name, new_dir_name, strlen(new_dir_name));

    // errc = sffs_write_inode(ctx, new_dir);
    // if(errc < 0)
    //     return -1; 

    // errc = sffs_init_direntry(ctx, ino_mem, new_dir);
    // if(errc < 0)
    //     return -1;

    // errc = sffs_add_direntry(ctx, ino_mem, new_direntry);
    // if(errc < 0)
    //     return -1; 

    // free(new_dir);
    // free(new_direntry);
    // return 0; 
}

int sffs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
			struct fuse_file_info *fi)
{
    // // In this example, we are providing a fixed directory listing for the root directory ("/")
    // if (strcmp(path, "/") != 0)
    //     return -ENOENT;

    // // Add entries to the directory listing using the filler function
    // filler(buf, ".", NULL, 0);     // Current directory entry
    // filler(buf, "..", NULL, 0);    // Parent directory entry
    // filler(buf, "file1.txt", NULL, 0);   // Example file entry
    // filler(buf, "file2.txt", NULL, 0);   // Another file entry

    struct fuse_context *fctx = fuse_get_context();
    sffs_context_t *ctx = (sffs_context_t *) fctx->private_data;

    sffs_err_t errc;
    struct sffs_inode_mem *ino_mem;
    errc = sffs_creat_inode(ctx, 0, SFFS_IFDIR, 0, &ino_mem);
    if(errc < 0)
        return -1;
    
    errc = sffs_read_inode(ctx, 0, ino_mem);
    if(errc < 0)
        return -1;

    struct sffs_data_block_info db_info;
    db_info.content = malloc(ctx->sb.s_block_size);
    if(!db_info.content)
        return SFFS_ERR_MEMALLOC;

    u32_t ino_blocks = ino_mem->ino.i_blks_count;
    u16_t accum_rec = 0;
    u16_t rec_len;
    struct sffs_direntry *dir_buf = (struct sffs_direntry *) 
        malloc(SFFS_MAX_DIR_ENTRY + 1);
    
    if(!buf)
        return SFFS_ERR_MEMALLOC;

    for(u32_t i = 0; i < ino_blocks; i++)
    {
        int flags = SFFS_GET_BLK_RD;
        errc = sffs_get_data_block_info(ctx, i, flags, &db_info, ino_mem);
        if(errc < 0)
            return errc;
        
        u8_t *dptr = (u8_t *) db_info.content;
        accum_rec = 0;

        do 
        {
            struct sffs_direntry *temp = (struct sffs_direntry *) dptr;
            rec_len = temp->rec_len;

            memcpy(dir_buf, temp, rec_len);
            size_t name_len = rec_len - SFFS_DIRENTRY_LENGTH;
            dir_buf->name[name_len] = 0;
            struct stat statbuf;
            statbuf.st_ino = 10;
            statbuf.st_mode = SFFS_IFDIR | SFFS_IRWXU | SFFS_IRGRP | SFFS_IWGRP | 
                SFFS_IXGRP | SFFS_IROTH | SFFS_IXOTH;
            statbuf.st_uid = 1000;

            if(dir_buf->file_type != 0)
            {
                size_t f_name_len = dir_buf->rec_len - SFFS_DIRENTRY_LENGTH;
                char *f_name = malloc(f_name_len + 1);
                if(!f_name)
                    return -1;
                
                memcpy(f_name, dir_buf->name, f_name_len);
                f_name[f_name_len] = 0;

                if(filler(buf, f_name, NULL, accum_rec) != 0)
                    return -1;
                free(f_name);
            }

            accum_rec += rec_len;
            dptr += rec_len;
        } while(accum_rec < ctx->sb.s_block_size);
    }

    return 0;
}

int sffs_read(const char *, char *, size_t, off_t, struct fuse_file_info *)
{
    return 0;
}

int sffs_opendir(const char *, struct fuse_file_info *) 
{
    printf("sffs_opendir\n");
    return 0;
}

#ifdef SFFS_THUMB

int sffs_readlink(const char *, char *, size_t) { THUMB_FUNC; }

int sffs_mknod(const char *, mode_t, dev_t) { THUMB_FUNC; }

int sffs_unlink(const char *) { THUMB_FUNC; }

int sffs_rmdir(const char *) { THUMB_FUNC; }

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
