#include <sffs.h>
#include <sffs_context.h>
#include <sffs_err.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>
#include <sys/statfs.h>

extern char *optarg;

// Number of avalaible options for SFFS
#define SFFS_OPTIONS    4
#define BYTE    1
#define KBYTE   BYTE * 1024
#define MBYTE   KBYTE * 1024
#define GBYTE   MBYTE * 1024

struct sffs_mount_opts
{
    char op;
    char *arg;
};

/**
 *  SFFS file system initialization code
*/
sffs_err_t __sffs_init(size_t fs_size)
{
    struct sffs_superblock sffs_sb;
    memset(&sffs_sb, 0, sizeof(struct sffs_superblock));
    blk32_t block_size = sffs_ctx.sb.s_block_size;

    /**
     *  The location of the superblock is at a fixed address
     *  which equals 1024 bytes from beginning of the disk
    */
    blk32_t sb_start = (1024 + sizeof(struct sffs_superblock)) >
        block_size ? 1 : 0;

    /**
     *  User specified number of reserved inodes. This value is a soft
     *  limit and does not affect the disk layout. It reserves 0 inodes by default.
     *  Reserved inodes always occupies first n slots in GIT table
    */
    blk32_t resv_inodes = SFFS_RESV_INODES;

    blk32_t total_blocks = (fs_size / block_size) - resv_inodes;
    blk32_t total_inodes = (total_blocks * block_size) / SFFS_INODE_RATIO;
    
    blk32_t GIT_size_blks = (total_inodes / (block_size / (SFFS_INODE_SIZE * 2))) + 1;
    blk32_t GIT_bitmap_bytes = (total_inodes / 8) + 1;
    blk32_t GIT_bitmap_blks = (GIT_bitmap_bytes / block_size) + 1;

    blk32_t meta_blks = ((sb_start + 1) + GIT_bitmap_blks + GIT_size_blks);

    blk32_t data_blocks = total_blocks - meta_blks;
    blk32_t data_bitmap_bytes = (data_blocks / 8) + 1;
    blk32_t data_bitmap_blks = (data_bitmap_bytes / block_size) + 1;

    // Number of data blocks is effectively reduced by a data bitmap
    data_blocks -= data_bitmap_blks;

    /**
     *  The size of the GIT must corrected.
     *  This is because first size of Global Inode Table has been evaluated
     *  without bitmaps
    */
    blk32_t grp_size_blks = SFFS_INODE_DATA_SIZE / 4;
    total_inodes = data_blocks / grp_size_blks;

    blk32_t result = meta_blks + data_bitmap_blks + data_blocks;
    if(result != total_blocks)
        return SFFS_ERR_INIT;
    
    /**
     *  After primary calculation has been done, superblock must be filled up
    */
    sffs_sb.s_block_size = block_size;
    sffs_sb.s_blocks_count = data_blocks;
    sffs_sb.s_free_blocks_count = data_blocks;
    sffs_sb.s_blocks_per_group = grp_size_blks;
    sffs_sb.s_group_count = total_inodes;
    sffs_sb.s_free_groups = total_inodes;
    sffs_sb.s_inodes_count = total_inodes;
    sffs_sb.s_free_inodes_count = total_inodes;
    sffs_sb.s_max_mount_count = SFFS_MAX_MOUNT;
    sffs_sb.s_max_inode_list = SFFS_MAX_INODE_LIST;
    sffs_sb.s_magic = SFFS_MAGIC;
    sffs_sb.s_features = 0;
    sffs_sb.s_error = 0;
    sffs_sb.s_prealloc_blocks = 0;
    sffs_sb.s_prealloc_dir_blocks = 0;
    sffs_sb.s_inode_size = sizeof(struct sffs_inode);
    sffs_sb.s_inode_block_size = SFFS_INODE_DATA_SIZE;
    time_t mount_time = time(NULL);
    sffs_sb.s_mount_count = mount_time;
    sffs_sb.s_write_time = mount_time;

    u32_t acc_address = sb_start + 1;

    /**
     *  Data bitmap location and size
    */
    sffs_sb.s_data_bitmap_start = acc_address;
    sffs_sb.s_data_bitmap_size = data_bitmap_blks;
    acc_address += data_bitmap_blks;

    /**
     *  GIT and GIT bitmap locations and size
    */
    sffs_sb.s_GIT_bitmap_start =  acc_address;
    sffs_sb.s_GIT_bitmap_size = GIT_bitmap_blks;
    acc_address += GIT_bitmap_blks;

    sffs_sb.s_GIT_start = acc_address;
    sffs_sb.s_GIT_size = GIT_size_blks;
    acc_address += GIT_size_blks;
    
    // In-memory superblock copy initialization
    sffs_ctx.sb = sffs_sb;

    /**
     *  SFFS superblock serialization
    */
    if(lseek64(sffs_ctx.disk_id, 1024, SEEK_SET) < 0)
        return SFFS_ERR_DEV_SEEK;
    
    if(write(sffs_ctx.disk_id, &sffs_sb, SFFS_SB_SIZE) == 0)
        return SFFS_ERR_DEV_WRITE;
    
    sffs_ctx.block_size = block_size;
    return 0;
}

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        fprintf(stderr, "mkfs.sffs: No device path specified\n");
        exit(EXIT_FAILURE);
    }

    /**
     *  First parse device and file system size
    */
    const char *device_argv = argv[argc - 2];
    const char *size_argv = argv[argc - 1];
    size_t size_argv_len = strlen(size_argv); 
    u32_t fs_size;

    if(isalpha(size_argv[size_argv_len - 1]))
    {
        char *sffs_arg = malloc(size_argv_len);
        if(!sffs_arg)
        {
            fprintf(stderr, "mkfs.sffs: Memory exhausted... abort()\n");
            abort();
        }
        memcpy(sffs_arg, size_argv, size_argv_len);

        char ch = sffs_arg[size_argv_len - 1]; 
        sffs_arg[size_argv_len - 1] = 0;
        fs_size = atoi(sffs_arg);

        if(ch == 'G' || ch == 'g')
            fs_size *= GBYTE;
        else if(ch == 'M' || ch == 'm')
            fs_size *= MBYTE;
        else if(ch == 'K' || ch == 'k')
            fs_size *= KBYTE;
        else
        {
            fprintf(stderr, "mkfs.sffs: Invalid file system size: %s\n", size_argv);
            exit(EXIT_FAILURE);
        }
        free(sffs_arg);
    }
    else 
        fs_size = atoi(size_argv);
    
    if(fs_size < 0)
    {
        fprintf(stderr, "mkfs.sffs: Invalid file system size: %s\n", size_argv);
        exit(EXIT_FAILURE);
    }

    if(access(device_argv, F_OK) == 0)
    {
        fprintf(stdout, "mkfs.sffs: The file [%s] already exist. Do you want to rewrite it? (y/n): ", device_argv);
        char answer;
        do 
        {
            if(fscanf(stdin, "%c", &answer) < 1)
                abort();
        } while(answer != 'n' && answer != 'y');

        if(answer == 'n')
            exit(EXIT_SUCCESS);
    }

    /**
     *  Initialize user-specified options
    */
    int opt;
    blk32_t block_size;
    blk32_t blocks_per_grp;
    u32_t inodes_ratio = SFFS_INODE_RATIO;

    while ((opt = getopt(argc, argv, "b:g:i:t:")) != -1) 
    {
        // Just primary initialization, check goes next
        switch (opt) 
        {
            case 'b':
                block_size = atoi(optarg);
            case 'g':
                blocks_per_grp = atoi(optarg); 
            case 'i':
                inodes_ratio = atoi(optarg);
            case 't':
                ;
            case '?':
            {
                if (optopt == 'f' || optopt == 'o')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                return 1;
            }
            default:
                abort();
        }
    }

    int flags = O_RDWR | O_CREAT | O_TRUNC;
    mode_t fmode = S_IRWXU | S_IRWXG | S_IRWXO;

    int fd;
    if((fd = open(device_argv, flags, fmode)) < 0)
        fprintf(stderr, "mkfs.sffs: Cannot creat SFFS image: %s", device_argv);

    sffs_ctx.disk_id = fd;
    if(ftruncate(fd, fs_size) < 0)
    {
        fprintf(stderr, "mkfs.sffs: Cannot creat %s image with specified size: %d", 
            device_argv, fs_size);
        remove(device_argv);
    }


    /*          SFFS arguments correction           */

    /**
     *  SFFS imposes restrictions on a file system block size.
     *  It must satisfy two mandatory conditions:
     *  - not greater than OS's page size
     *  - be power of two
     *  
     *  The one optional condition after two mandatory, is that file system's block size
     *  desirably must be 1024 < block_size < 4096 as SFFS performance is optimized
     *  to work within this range
    */
    if(block_size == 0)
    {
        struct statfs cwd_fs;
        if(statfs(device_argv, &cwd_fs) < 0)
            return SFFS_ERR_DEV_STAT;

        block_size = cwd_fs.f_bsize;

        if(!(block_size > 0 && (block_size & (block_size - 1)) == 0))
            return SFFS_ERR_INVBLK;
        else if(block_size > getpagesize())
            return SFFS_ERR_INVBLK; 
    }
    
    if(!(block_size > 0 && (block_size & (block_size - 1)) == 0))
    {
        fprintf(stderr, "mkfs.sffs: Block size of underlying device is \
            not a power of two\n");
    }
    else if(block_size > getpagesize())
    {
        fprintf(stderr, "mkfs.sffs: Block size of underlying device is \
            bigger than OS's page size\n");
    }
    else if(block_size < 1024 || block_size > 4096)
    {
        fprintf(stderr, "mkfs.sffs: SFFS\'s block size within an inefficient range: \
            1024 < %d < 4096\n", block_size);
    }

    if((sffs_ctx.cache = malloc(4096)) == 0)
        return SFFS_ERR_MEMALLOC;
    memset(sffs_ctx.cache, 0, block_size);
    sffs_ctx.sb.s_block_size = block_size;
    sffs_err_t errc = __sffs_init(fs_size);
    if(errc < 0)
    {
        fprintf(stderr, "mkfs.sffs: Error during SFFS image initialization\n");
        abort();
    }

    printf("File system successfully created\n");
    printf("SFFS_PATH: %s\n", device_argv);
    printf("SFFS_SIZE: %d\n", fs_size);
    printf("SFFS_BLOCK_SIZE: %d\n", sffs_ctx.sb.s_block_size);
    printf("SFFS_BLOCKS_COUNT: %d\n", sffs_ctx.sb.s_blocks_count);
    printf("SFFS_INODES_COUNT: %d\n", sffs_ctx.sb.s_inodes_count);
    exit(EXIT_SUCCESS);
}