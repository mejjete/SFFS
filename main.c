/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#include "sffs_fuse.h"
#include "sffs_err.h"
#include "sffs.h"

/** 
 *  Define only a limited set of basic operations and let 
 *  FUSE silently define remaining
*/
#ifndef SFFS_THUMB

struct fuse_operations sffs_ops = 
{
    .getattr        = sffs_getattr,
    .mkdir          = sffs_mkdir,
    .readdir        = sffs_readdir,
    .init           = sffs_init,
    .destroy        = sffs_destroy,
    .statfs         = sffs_statfs
};

#else

static struct fuse_operations sffs_ops = 
{
    .getattr        = sffs_getattr,
    .readlink       = sffs_readlink,
    .mknod          = sffs_mknod,
    .mkdir          = sffs_mkdir,
    .unlink         = sffs_unlink,
    .rmdir          = sffs_rmdir,
    .symlink        = sffs_symlink,
    .rename         = sffs_rename,
    .link           = sffs_link,
    .chmod          = sffs_chmod,
    .chown          = sffs_chown,
    .truncate       = sffs_truncate,
    .open           = sffs_open,
    .read           = sffs_read,
    .write          = sffs_write,
    .statfs         = sffs_statfs,
    .flush          = sffs_flush,
    .release        = sffs_release,
    .fsync          = sffs_fsync,
    .setxattr       = sffs_setxattr,
    .getxattr       = sffs_getxattr,
    .listxattr      = sffs_listxattr,
    .removexattr    = sffs_removexattr,
    .opendir        = sffs_open, 
    .readdir        = sffs_readdir,
    .releasedir     = sffs_releasedir,
    .fsyncdir       = sffs_fsync,
    .init           = sffs_init,
    .destroy        = sffs_destroy,
    .create         = sffs_create,
    .ftruncate      = sffs_ftruncate,
    .fgetattr       = sffs_fgetattr,
    .lock           = sffs_lock,
    .utimens        = sffs_utimens,
    .bmap           = sffs_bmap,
    .ioctl          = sffs_ioctl,
    .poll           = sffs_poll,
    .write_buf      = sffs_write_buf,
    .read_buf       = sffs_read_buf,
    .flock          = sffs_flock,
    .fallocate      = sffs_fallocate,
};

#endif // SFFS_THUMB

static const struct fuse_opt sffs_option_spec[] = 
{
    SFFS_OPT_INIT("--fs-size=%d", fs_size),
    SFFS_OPT_INIT("--log-file=%s", log_file),
    FUSE_OPT_END
};

sffs_context_t sffs_ctx;

int main(int argc, char **argv)
{
    struct fuse_args sffs_args = FUSE_ARGS_INIT(argc, argv);
    struct sffs_options options;
    memset(&options, 0, sizeof(options));

    if (fuse_opt_parse(&sffs_args, &options, sffs_option_spec, NULL) == -1)
        err_sys("sffs: Cannot parse cmd arguments\n");
    sffs_ctx.opts = options;
    
    /**
     *  Current working directory used by sffs as a determination point
     *  to get the block size and other "device" specific characteristics.
     *  Also, sffs creates a file system image in cwd so it must also be reachable
    */
    char *cwd;
    if((cwd = malloc(4096)) == NULL)
        err_sys("sffs: Cannot allocate mmemory\n");

    if(getcwd(cwd, 4096) < 0)
        err_sys("sffs: Cannot get current working directory\n");

    // Initialize log file
    int log;
    if((log = open("fslog", O_CREAT | O_TRUNC | O_RDWR, 
            S_IRWXU | S_IRWXG | S_IRWXO)) < 0)
        err_no_log();

    sffs_ctx.cwd = cwd;
    sffs_ctx.log_id = log;

    fuse_main(sffs_args.argc, sffs_args.argv, &sffs_ops, NULL); 
    
    // free resources
    free(sffs_ctx.cwd);
    return 0;
}