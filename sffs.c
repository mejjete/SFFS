/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

/**
 *  Implementations of a sffs core functions
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
sffs_err_t __sffs_init()
{
    struct sffs_superblock sffs_sb;
    memset(&sffs_sb, 0, sizeof(struct sffs_superblock));

    /**
     *  Determining the block size in FUSE only goes to determining 
     *  block size of the underlying device or file system
    */
    struct statfs cwd_fs;
    if(statfs(sffs_ctx.cwd, &cwd_fs) < 0)
        return SFFS_ERR_DEV_STAT;

    blk32_t block_size = cwd_fs.f_bsize;

    /**
     *  SFFS imposes restrictions on a file system block size.
     *  It must satisfy two mandatory conditions:
     *  - not greater than OS's page size
     *  - be power of two
    */
    if(!(block_size > 0 && (block_size & (block_size - 1)) == 0))
        return SFFS_ERR_INVBLK;
    else if(block_size > getpagesize())
        return SFFS_ERR_INVBLK; 

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

    blk32_t total_blocks = (sffs_ctx.opts.fs_size / block_size) - resv_inodes;
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
    
    if((sffs_ctx.cache = malloc(4096)) == 0)
        return SFFS_ERR_MEMALLOC;

    memset(sffs_ctx.cache, 0, block_size);

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

sffs_err_t sffs_read_sb(u8_t sb_id, struct sffs_superblock *sb)
{    
    if(lseek64(sffs_ctx.disk_id, 1024, SEEK_SET) < 0)
        return SFFS_ERR_DEV_SEEK;
    
    if(read(sffs_ctx.disk_id, sb, SFFS_SB_SIZE) < 0)
        return SFFS_ERR_DEV_WRITE;
    
    return 0;
}

sffs_err_t sffs_write_sb(u8_t sb_id, struct sffs_superblock *sb)
{
    if(lseek64(sffs_ctx.disk_id, 1024, SEEK_SET) < 0)
        return SFFS_ERR_DEV_SEEK;
    
    // Update superblock directly because in-memory version always up-to-date
    if(write(sffs_ctx.disk_id, sb, 1024) < 0)
        return SFFS_ERR_DEV_WRITE;
    
    return 0;
}

sffs_err_t sffs_creat_inode(ino32_t ino_id, mode_t mode, int flags,
    struct sffs_inode_mem **ino_mem)
{
    if(ino_mem == NULL)
        return SFFS_ERR_INVARG;

    if((*ino_mem = malloc(sffs_ctx.sb.s_inode_size + 
        sffs_ctx.sb.s_inode_block_size)) == NULL)
        return SFFS_ERR_MEMALLOC;

    struct sffs_inode *inode = &((*ino_mem)->ino);
    
    // Inode's mode must have only 1 bit set
    mode_t md = (mode & SFFS_IFMT) >> 12;    
    if(!((md & (md - 1)) == 0 && md != 0))
        return SFFS_ERR_INVARG;

    inode->i_inode_num = ino_id;
    inode->i_next_entry = 0;
    inode->i_link_count = 0;
    inode->i_flags = flags;
    inode->i_mode = mode;
    inode->i_blks_count = 0;
    inode->i_bytes_rem = 0;
    inode->i_uid_owner = getuid();
    inode->i_gid_owner = getgid();
    inode->i_list_size = 1;
    inode->i_last_lentry = ino_id;

    // Time constants
    time_t tm = time(NULL);
    inode->tv.t32.i_crt_time = tm;
    inode->tv.t32.i_mod_time = tm;
    inode->tv.t32.i_acc_time = tm;
    inode->tv.t32.i_chg_time = tm;

    return 0;
}

sffs_err_t sffs_write_inode(struct sffs_inode_mem *ino_mem)
{
    if(ino_mem == NULL)
        return SFFS_ERR_INVARG;

    struct sffs_inode *inode = &ino_mem->ino;

    sffs_err_t errc;
    ino32_t ino = inode->i_inode_num;
    
    ino32_t ino_entry_size = sffs_ctx.sb.s_inode_block_size + 
        sffs_ctx.sb.s_inode_size;

    ino32_t ino_per_block = sffs_ctx.block_size / ino_entry_size;
    blk32_t git_block = ino / ino_per_block;
    blk32_t block_offset = (ino % ino_per_block) * 
        ino_entry_size;

    blk32_t ino_block = sffs_ctx.sb.s_GIT_start + git_block;

    errc = sffs_read_blk(ino_block, sffs_ctx.cache, 1);
    if(errc < 0)
        return errc;
    
    memcpy(sffs_ctx.cache + block_offset, ino_mem, ino_entry_size);

    // First update GIT table
    errc = sffs_write_blk(ino_block, sffs_ctx.cache, 1);
    if(errc < 0)
        return errc;
    
    // Second: update superblock
    sffs_ctx.sb.s_free_inodes_count--;

    // Third: update bitmap
    return sffs_set_GIT_bm(ino);
}

sffs_err_t sffs_read_inode(ino32_t ino_id, struct sffs_inode_mem *ino_mem)
{
    if(!ino_mem)
        return SFFS_ERR_INVARG;

    struct sffs_inode *inode = &ino_mem->ino;

    if(sffs_check_GIT_bm(ino_id) != 0)
    {
        sffs_err_t errc;
        ino32_t ino_entry_size = sffs_ctx.sb.s_inode_block_size + 
            sffs_ctx.sb.s_inode_size;

        ino32_t ino_per_block = sffs_ctx.block_size / ino_entry_size;
        blk32_t git_block = ino_id / ino_per_block;
        blk32_t block_offset = (ino_id % ino_per_block) * 
            ino_entry_size;

        blk32_t ino_block = sffs_ctx.sb.s_GIT_start + git_block;

        errc = sffs_read_blk(ino_block, sffs_ctx.cache, 1); 
        if(errc < 0)
            return errc;
        
        memcpy(inode, sffs_ctx.cache + block_offset, ino_entry_size);
        return true;
    }
    else
        return false;
}

sffs_err_t sffs_alloc_inode(ino32_t *ino_id, mode_t mode)
{
    if(!ino_id)
        return SFFS_ERR_NOSPC;

    ino32_t resv_inodes = sffs_ctx.sb.s_inodes_reserved;
    ino32_t max_inodes = sffs_ctx.sb.s_inodes_count - resv_inodes;

    for(int i = resv_inodes; i < max_inodes; i++)
    {
        sffs_err_t errc = sffs_check_GIT_bm(i);
        if(errc < 0)
            return errc;
        
        if(errc == false)
        {
            *ino_id = i;
            return true;
        }
    }

    return SFFS_ERR_NOSPC;
}

sffs_err_t sffs_alloc_inode_list(ino32_t size, struct sffs_inode_mem *ino_mem)
{
    if(!ino_mem || size == 0)
        return SFFS_ERR_INVARG;

    // Maximum inode entry list has been reached
    if(SFFS_MAX_INODE_LIST != 0)
        if(ino_mem->ino.i_list_size + size > SFFS_MAX_INODE_LIST)
            return SFFS_ERR_NOSPC;

    // No free inodes to allocate
    if(size > sffs_ctx.sb.s_free_inodes_count)
        return SFFS_ERR_NOSPC;
    
    struct sffs_inode *inode = &ino_mem->ino;
    ino32_t *list_entries = malloc(sizeof(ino32_t) * size);
    bool seq_list = true;

    // Try to allocate inode list entries right next to the base inode
    ino32_t ino = inode->i_inode_num;
    blk32_t ino_per_block = sffs_ctx.block_size / (SFFS_INODE_SIZE + 
        SFFS_INODE_DATA_SIZE);
    size_t ino_id_within_block = ino % ino_per_block;
    
    if(ino_id_within_block + size > ino_per_block)
        goto non_seq_alloc; 

    for(int i = 0; i < size; i++)
    {
        /**
         *  Take the last inode list entry to ensure that
         *  inode list is sequential as much as possible
        */
        ino32_t next_entry = inode->i_last_lentry + i + 1;
        
        if(sffs_check_GIT_bm(next_entry) != 0)
        {
            seq_list = false;
            break;
        }
        
        list_entries[i] = next_entry;
    }

    if(seq_list == true)
        goto alloc_done;

/**
 *  This label means that file system cannot sequentially allocate
 *  inode list and will try another (random) allocation technique
*/
non_seq_alloc:
    ino32_t free_inodes = sffs_ctx.sb.s_free_inodes_count;
    ino32_t allocated = 0;

    for(int i = 0; i < free_inodes && allocated < size; i++)
    {
        if(sffs_check_GIT_bm(i) == false)
        {
            list_entries[allocated] = i;
            allocated++; 
        }
    }

    if(allocated < size)
        return SFFS_ERR_FS;

/**
 *  This label means that list_entries are full of requested entries and
 *  further must be pushed on-disk
 */ 
alloc_done:
    struct sffs_inode_mem *current_inode;
    sffs_err_t errc = sffs_creat_inode(0, SFFS_IFREG, 0, &current_inode);
    if(errc < 0)
        return errc;

    if(!current_inode)
        return SFFS_ERR_MEMALLOC;

    // Create on-disk list of inode entries
    for(int i = 0; i < size; i++)
    {        
        current_inode->ino.i_inode_num = list_entries[i];
        current_inode->ino.i_next_entry = i + 1 == size ? 0 : list_entries[i + 1];

        sffs_err_t errc = sffs_write_inode(current_inode);
        if(errc < 0)
            return errc;
    }

    /**
     *  Add newly allocated inode entries to inode list
    */
    struct sffs_inode_mem *buf_inode;
    errc = sffs_creat_inode(0, SFFS_IFREG, 0, &buf_inode);
    if(errc < 0)
        return errc;

    if(inode->i_last_lentry != inode->i_inode_num)
    {
        errc = sffs_read_inode(inode->i_last_lentry, buf_inode);
        if(errc < 0)
            return errc;
        
        struct sffs_inode *buf = &buf_inode->ino;
        buf->i_next_entry = list_entries[0];

        errc = sffs_write_inode(buf_inode);
        if(errc < 0)
            return errc;
    }
    else 
        inode->i_next_entry = list_entries[0];

    inode->i_list_size += size;
    inode->i_last_lentry = list_entries[size - 1];

    errc = sffs_write_inode(ino_mem);
    if(errc < 0)
        return errc;

    sffs_ctx.sb.s_free_inodes_count -= size;

    free(list_entries);
    free(buf_inode);
    free(current_inode);
    return 0;
}

sffs_err_t sffs_get_data_block_info(blk32_t block_number, int flags, 
    struct sffs_data_block_info *db_info, struct sffs_inode_mem *ino_mem)
{
    if(!ino_mem || !db_info)
        return SFFS_ERR_INVARG;
    
    if(ino_mem->ino.i_blks_count < block_number)
        return SFFS_ERR_INVARG;
    
    sffs_err_t errc;
    bool read_blk = false;
    blk32_t block_id = block_number;

    // Control flags that change behavior of this handler
    if(flags != 0)
    {
        if((flags & SFFS_GET_BLK_LT) == SFFS_GET_BLK_LT)
            block_id = ino_mem->ino.i_blks_count - 1;
        if((flags & SFFS_GET_BLK_RD) == SFFS_GET_BLK_RD)
            read_blk = true;
    }

    u32_t ino_size = sffs_ctx.sb.s_inode_size;
    u32_t ino_data_size = sffs_ctx.sb.s_inode_block_size;
    u32_t ino_entry_size = ino_size + ino_data_size;
    u32_t pr_ino_blks = ino_data_size / sizeof(blk32_t);
    u32_t supp_ino_blks = (ino_entry_size - SFFS_INODE_LIST_SIZE) / sizeof(blk32_t);

    u32_t blk_ino;
    struct sffs_inode_mem *buf;
    errc = sffs_creat_inode(0, SFFS_IFREG, 0, &buf);
    if(errc < 0)
            return errc;

    if(block_number < pr_ino_blks)
    {
        buf = ino_mem;
        blk_ino = block_number;
    }
    else 
    {
        block_id -= pr_ino_blks;
        u32_t supp_ino_id = block_id / supp_ino_blks;
        blk_ino = block_id % supp_ino_blks; 
        if(blk_ino != 0)
            supp_ino_id++;

        // Inode list is smaller than requested block's inode list entry
        if(supp_ino_id > ino_mem->ino.i_list_size)
            return SFFS_ERR_INVARG;

        ino32_t supp_ino = ino_mem->ino.i_next_entry;
        for(u32_t i = 0; i < supp_ino_id && supp_ino != 0; i++)
        {
            errc = sffs_read_inode(supp_ino, buf);
            if(errc < 0)
                return errc;
            supp_ino = buf->ino.i_next_entry;
        }
    }

    struct sffs_inode_list *ino_list = (struct sffs_inode_list *) buf;
    db_info->block_id = *(ino_list->blks + blk_ino);
    db_info->inode_id = buf->ino.i_inode_num;
    db_info->list_id = blk_ino;
    db_info->flags = 0;             // reserved field

    // Read the block itself if requested
    if(read_blk)
    {
        db_info->blks = malloc(sizeof(blk32_t) * sffs_ctx.sb.s_block_size);
        if(!db_info->blks)
            return SFFS_ERR_MEMALLOC;

        errc = sffs_read_blk(db_info->block_id, db_info->blks, 1);
        if(errc < 0)
            return errc;
    }
    else 
        db_info->blks = NULL;
    
    free(buf);
    return 0;
}