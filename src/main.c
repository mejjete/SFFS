/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#include <sffs_fuse.h>
#include <sffs_err.h>
#include <sffs.h>
#include <sffs_device.h>

static const struct fuse_opt sffs_option_spec[] = 
{
    SFFS_OPT_INIT("--fs-size=%d", fs_size),
    SFFS_OPT_INIT("--log-file=%s", log_file),
    FUSE_OPT_END
};

sffs_context_t sffs_ctx;
sffs_err_t sffs_expose_bitmap(blk32_t bitmap, size_t size);
sffs_err_t sffs_expose_superblock(u8_t sb_id);
sffs_err_t sffs_alloc_test_1();

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
        err_sys("sffs: Cannot allocate memory\n");

    if(getcwd(cwd, 4096) != NULL)
        err_sys("sffs: Cannot get current working directory\n");

    // Initialize log file
    int log;
    if((log = open("fslog", O_CREAT | O_TRUNC | O_RDWR, 
            S_IRWXU | S_IRWXG | S_IRWXO)) < 0)
        err_no_log();

    sffs_ctx.cwd = cwd;
    sffs_ctx.log_id = log;

#if 1
    sffs_ctx.cache = malloc(4096);
    sffs_ctx.disk_id = open(".__sffs_image", O_RDWR);
    sffs_read_sb(0, &sffs_ctx.sb);
    sffs_ctx.block_size = sffs_ctx.sb.s_block_size;
#if 0
    struct sffs_inode_mem *inode;
    sffs_err_t errc;

    errc = sffs_creat_inode(10, SFFS_IFREG, 0, &inode);
    // errc = sffs_write_inode(inode);
    errc = sffs_read_inode(10, inode);

    // ino32_t new_inode;
    // mode_t mode = SFFS_IFREG;
    // sffs_alloc_inode(&new_inode, mode);
    // sffs_creat_inode(new_inode, mode, 0, &inode);

    errc = sffs_alloc_inode_list(5, inode);
    sffs_expose_bitmap(sffs_ctx.sb.s_GIT_bitmap_start, 64);
#endif

#endif
    printf("GIT BITMAP:\n");
    sffs_expose_bitmap(sffs_ctx.sb.s_GIT_bitmap_start, 64);

    printf("DATA BITMAP:\n");
    sffs_expose_bitmap(sffs_ctx.sb.s_data_bitmap_start, 64);

    // fuse_main(sffs_args.argc, sffs_args.argv, &sffs_ops, NULL); 
    
    // free resources
    free(sffs_ctx.cwd);
    return 0;
}

// sffs_err_t sffs_alloc_test_1()
// {
//     sffs_err_t errc;
//     struct sffs_inode_mem *ino_mem;

//     errc = sffs_creat_inode(0, SFFS_IFREG, 0, &ino_mem);
//     if(errc < 0)
//         printf("ERROR\n");
    
//     errc = sffs_read_inode(0, ino_mem);
//     if(errc < 0)
//         printf("ERROR\n");
    
//     errc = sffs_alloc_data(1, ino_mem);
//     if(errc < 0)
//         printf("ERROR\n");
//     return 0;
// }

sffs_err_t sffs_expose_bitmap(blk32_t bitmap, size_t size)
{
    sffs_err_t errc = sffs_read_blk(bitmap, sffs_ctx.cache, 1);
    if(errc < 0)
        return errc;
    
    u8_t *byte = (u8_t *) sffs_ctx.cache;
    u32_t byte_id = 0;

    printf("%-10d", byte_id * 8);
    for(int i = 0; i < size; i++)
    {
        if((i % 4) == 0 && i != 0)
            printf(" ");

        if((i % 16) == 0 && i != 0)
        {
            printf("\n");
            printf("%-10d", byte_id * 8);
        }

        u8_t bit_id = i % 8;
        u8_t bt = (*byte) >> bit_id;
        printf("%d", bt & 1);

        if(((i + 1) % 8) == 0)
        {
            ++byte;
            byte_id++;
        }
    }

    printf("\n");
    return 0;
}

sffs_err_t sffs_expose_superblock(u8_t sb_id)
{
    printf("Total blocks: %10d\n", sffs_ctx.sb.s_blocks_count);
    printf("Total inodes: %10d\n", sffs_ctx.sb.s_inodes_count);
    printf("Free blocks:  %10d\n", sffs_ctx.sb.s_free_blocks_count);
    printf("Free inodes:  %10d\n", sffs_ctx.sb.s_free_inodes_count);

    return 0;
}