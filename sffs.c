/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

/**
 *  Implementation of a sffs core functions
*/

#include <string.h>
#include <sys/vfs.h>
#include "sffs_context.h"
#include "sffs.h"
#include "sffs_err.h"
#include "sffs_device.h"
#include "time.h"

/**
 *  SFFS file system initialization code
*/
void __sffs_init()
{
    struct sffs_superblock sffs_sb;
    memset(&sffs_sb, 0, sizeof(struct sffs_superblock));

    /**
     *  Determining the block size in FUSE only goes to 
     *  determining block size of the underlying device 
     *  or file system 
    */
    struct statfs cwd_fs;
    if(statfs(sffs_ctx.cwd, &cwd_fs) < 0)
        err_sys("sffs: Cannot obtain block size of underlying device\n");

    blk32_t block_size = cwd_fs.f_bsize;

    /**
     *  SFFS imposes restrictions on a file system block size.
     *  It must satisfy two mandatory conditions:
     *  - not greater than OS's page size
     *  - be power of two
     *  
     *  The one optional condition after two mandatory, is that file system's block size
     *  desirably must be 1024 < block_size < 4096 as SFFS performance is optimized
     *  to work within this range
     *  
    */
    if(!(block_size > 0 && (block_size & (block_size - 1)) == 0))
    {
        err_sys("sffs: Block size of underlying device is not a power of two\n");
    }
    else if(block_size > getpagesize())
    {
        err_sys("sffs: Block size of underlying device is bigger than OS's page size\n");
    }
    else if(block_size < 1024 || block_size > 4096)
    {
        err_msg("sffs: SFFS\'s block size within an inefficient range: 1024 < %d < 4096\n", block_size);
    }   

    blk32_t total_blocks = sffs_ctx.opts.fs_size / block_size;
    blk32_t total_inodes = (total_blocks * block_size) / SFFS_INODE_RATIO;
    blk32_t GIT_size_blks = (total_inodes / (block_size / SFFS_INODE_ENTRY_SIZE)) + 1;
    blk32_t GIT_bitmap_bytes = (total_inodes / 8) + 1;
    blk32_t GIT_bitmap_blks = (GIT_bitmap_bytes / block_size) + 1;
    
    /**
     *  The location of the superblock is at a fixed location
     *  which equals 1024 bytes from beginnig of the disk
    */
    blk32_t superblock_start = (1024 + sizeof(struct sffs_superblock)) >
        block_size ? 1 : 0;

    /**
     *  To count the number of meta data blocks, we should add 2 first blocks 
     *  (1 boot blocks and 1 superblock) + GIT size +  GIT bitmaps size
    */
    blk32_t meta_blks = ((superblock_start + 1) + GIT_bitmap_blks + GIT_size_blks);

    blk32_t data_blocks = total_blocks - meta_blks;
    blk32_t data_bitmap_bytes = (data_blocks / 8) + 1;
    blk32_t data_bitmap_blks = (data_bitmap_bytes / block_size) + 1;

    // Number of data blocks is effectively reduced by a data bitmap
    data_blocks -= data_bitmap_blks;

    blk32_t result = meta_blks + data_bitmap_blks + data_blocks;
    if(result != total_blocks)
        err_sys("sffs_init: Final number of blocks not equal total number of blocks\n");
    
    /**
     *  After primary calculation has been done, superblock must be filled up
    */
    sffs_sb.s_block_size = block_size;
    sffs_sb.s_blocks_count = total_blocks;
    sffs_sb.s_free_blocks_count = total_blocks;
    sffs_sb.s_blocks_per_group = 0;
    sffs_sb.s_inodes_count = total_inodes;
    sffs_sb.s_free_inodes_count = total_inodes;
    sffs_sb.s_mount_time = 0;
    sffs_sb.s_max_mount_count = SFFS_MAX_MOUNT;
    sffs_sb.s_max_inode_list = SFFS_MAX_INODE_LIST;
    sffs_sb.s_magic = SFFS_MAGIC;
    sffs_sb.s_features = 0;
    sffs_sb.s_error = 0;
    sffs_sb.s_prealloc_blocks = 0;
    sffs_sb.s_prealloc_dir_blocks = 0;
    sffs_sb.s_inode_size = SFFS_INODE_SIZE;
    sffs_sb.s_inode_block_size = SFFS_INODE_DATA_SIZE;
    sffs_sb.s_GIT_reserved = 0;
    sffs_sb.s_GIT_bitmap_reserved = 0;
    time_t mount_time = time(NULL);
    sffs_sb.s_mount_count = mount_time;
    sffs_sb.s_write_time = mount_time;

    u32_t acc_address = superblock_start + 1;

    /**
     *  Data bitmap location and size
    */
    sffs_sb.s_data_bitmap_start = acc_address;
    sffs_sb.s_data_bitmap_size = data_bitmap_blks;
    acc_address += data_bitmap_blks;

    /**
     *  GIT and GIT bitmap locations and sizes
     *  GIT may have a reserved space for a future allocations
    */
    sffs_sb.s_GIT_bitmap_start =  acc_address;
    sffs_sb.s_GIT_bitmap_size = GIT_bitmap_blks + sffs_sb.s_GIT_bitmap_reserved;
    acc_address += GIT_bitmap_blks;

    sffs_sb.s_GIT_start = acc_address;
    sffs_sb.s_GIT_size = GIT_size_blks + sffs_sb.s_GIT_reserved;
    acc_address += GIT_size_blks;

    sffs_ctx.sb = sffs_sb;

    if((sffs_ctx.cache = malloc(data_bitmap_bytes)) == NULL)
        err_sys("sffs: Cannot allocate memory\n");
    
    memset(sffs_ctx.cache, 0, data_bitmap_bytes);
    
    if(write(sffs_ctx.disk_id, sffs_ctx.cache, data_bitmap_bytes) < 0)
        err_sys("sffs: Cannot write to underlying device")
    

    /**
     *  SFFS superblock serializaing
    */
    if(lseek64(sffs_ctx.disk_id, 1024, SEEK_SET) < 0)
        err_sys("sffs: Cannot seek underlying device\n");
    
    if(write(sffs_ctx.disk_id, &sffs_sb, SFFS_SB_SIZE) == 0)
        err_sys("sffs: Cannot write to underlying device\n");
}

void sffs_read_sb(u8_t sb_id, struct sffs_superblock *sb)
{    
    if(lseek64(sffs_ctx.disk_id, 1024, SEEK_SET) < 0)
        err_sys("sffs: Cannot seek underlying device\n");
    
    if(read(sffs_ctx.disk_id, sb, SFFS_SB_SIZE) < 0)
        err_sys("sffs: Cannot write to underlying device\n");
}