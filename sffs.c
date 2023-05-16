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
    if(md == 0)
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

    u32_t blk_off;
    u32_t blk_ino;
    u32_t *blk_ptr;
    struct sffs_inode_mem *buf;
    errc = sffs_creat_inode(0, SFFS_IFREG, 0, &buf);
    if(errc < 0)
        return errc;

    if(block_number < pr_ino_blks)
    {
        blk_ptr = ino_mem->blks;
        blk_off = block_number;
        blk_ino = ino_mem->ino.i_inode_num;
    }
    else 
    {
        block_id -= pr_ino_blks;
        u32_t supp_ino_id = (block_id / supp_ino_blks) + 1;
        blk_off = block_id % supp_ino_blks; 

        // Inode list is smaller than requested block's inode list entry
        if(supp_ino_id > ino_mem->ino.i_list_size)
            return SFFS_ERR_INVARG;

        ino32_t supp_ino = ino_mem->ino.i_next_entry;
        for(u32_t i = 0; i < supp_ino_id && supp_ino != 0; i++)
        {
            errc = sffs_read_inode(supp_ino, buf);
            if(errc < 0)
                return errc;
            struct sffs_inode_list *list = (struct sffs_inode_list *) buf;
            blk_ptr = list->blks;
            blk_ino = list->i_inode_num;
            supp_ino = buf->ino.i_next_entry;
        }
    }

    db_info->block_id = *(blk_ptr + blk_off);
    db_info->inode_id = blk_ino;
    db_info->list_id = blk_off;
    db_info->flags = 0;             // reserved field

    free(buf);

    // Read the block itself if requested
    if(read_blk)
    {
        db_info->content = malloc(sizeof(blk32_t) * sffs_ctx.sb.s_block_size);
        if(!db_info->content)
            return SFFS_ERR_MEMALLOC;

        errc = sffs_read_blk(db_info->block_id, db_info->content, 1);
        if(errc < 0)
            return errc;
    }
    else 
        db_info->content = NULL;
    return 0;
}

/**
 *  Reads group bitmap (typically 32-bit value) from bitmap
*/
sffs_err_t __get_group_bitmap(blk32_t bm_start, blk32_t group_bm, bmap_t *result)
{
    if(bm_start != sffs_ctx.sb.s_data_bitmap_start &&
        bm_start != sffs_ctx.sb.s_GIT_bitmap_start)
        return SFFS_ERR_INVARG;
    
    if(!result)
        return SFFS_ERR_INVARG;
    
    if(group_bm == sffs_ctx.sb.s_data_bitmap_start)
        if(group_bm > sffs_ctx.sb.s_group_count)
            return SFFS_ERR_INVARG;
    
    u32_t temp = sffs_ctx.sb.s_block_size * 8;
    u32_t grp_size = sffs_ctx.sb.s_blocks_per_group / 8;

    blk32_t blk_id = group_bm / temp;
    blk32_t grp_id = group_bm % temp; 

    sffs_err_t errc;
    errc = sffs_read_blk(bm_start + blk_id, sffs_ctx.cache, 1);
    if(errc < 0)
        return errc;

    u8_t *ca = (u8_t *) sffs_ctx.cache;
    blk32_t *res = (blk32_t *) (ca + (grp_id * grp_size));
    *result = *res;
    return 0;
}

static bool __find_block(blk32_t *blks, size_t size, blk32_t block)
{
    if(!blks)
        return false;
    
    for(u32_t i = 0; i < size; i++)
        if(blks[i] == block)
            return true;
    return false;
}

sffs_err_t sffs_alloc_data_blocks(size_t blk_count, struct sffs_inode_mem *ino_mem)
{
    if(!ino_mem)
        return SFFS_ERR_INVARG;

    struct sffs_inode *inode = &(ino_mem->ino);
    sffs_err_t errc;

    /**
     *  Depending on the settings, SFFS could preallocate some amount
     *  of data blocks.
    */
    blk32_t prealloc = 0;
    if(SFFS_ISREG(inode->i_mode))
        prealloc = sffs_ctx.sb.s_prealloc_blocks;
    else if(SFFS_ISDIR(inode->i_mode))
        prealloc = sffs_ctx.sb.s_prealloc_dir_blocks;

    /**
     *  Check if we could preallocate default amount, if not, just
     *  allocate as is
    */
    blk32_t alloc_blocks = blk_count + prealloc;

    if(alloc_blocks > sffs_ctx.sb.s_free_blocks_count)
    {
        if(blk_count > sffs_ctx.sb.s_free_blocks_count)
            return SFFS_ERR_NOSPC;
        else 
            alloc_blocks = blk_count;
    }

    // Allocate inode list if needed
    u32_t ino_size = sffs_ctx.sb.s_inode_size;
    u32_t ino_data_size = sffs_ctx.sb.s_inode_block_size;
    u32_t ino_entry_size = ino_size + ino_data_size;
    u32_t pr_inode_blks = ino_data_size / sizeof(blk32_t);
    u32_t supp_ino_blks = (ino_entry_size - SFFS_INODE_LIST_SIZE) 
        / sizeof(blk32_t);
    
    u32_t supp_ino_count = inode->i_list_size - 1;
    u32_t supp_ino_max_blks = supp_ino_count * supp_ino_blks;
    u32_t free_blks = (pr_inode_blks + supp_ino_max_blks) - 
        inode->i_blks_count;
    
    /**
     *  Before allocating new inode list, keep current inode
     *  list last entry. This value will come in handy during
     *  serialization of a block ids
    */
    u32_t last_entry = inode->i_last_lentry;

    if(free_blks < alloc_blocks)
    {
        u32_t clear_blks = alloc_blocks - free_blks;
        ino32_t supp_inodes = clear_blks / supp_ino_blks;
        if((clear_blks % supp_ino_blks) != 0)
            supp_inodes++;
        
        errc = sffs_alloc_inode_list(supp_inodes, ino_mem);
        if(errc < 0)
            return errc;
        last_entry = inode->i_next_entry;
    }

    blk32_t *new_blocks = malloc(sizeof(blk32_t) * alloc_blocks);
    if(!new_blocks)
        return SFFS_ERR_MEMALLOC;
    u32_t allocated = 0;
    u32_t allocated_grps = 0;

    /*                      Data blocks allocation                              */
    /*                                                                          */
    /* The allocation of a data blocks goes in three mutually exclusive steps.  */
    /* Step one: is to try to extend an inode, literally. SFFS will fetch the   */
    /* the last block of a given inode and try to allocate following blocks if  */
    /* they are free. SFFS cannot check parent of the block (because it would   */
    /* require to traverse the whole inode table) thus if blocks behind the     */
    /* last blocks is occupied, file system will try to allocate block within   */
    /* same group block. If there're no free blocks left, second step goes here.*/
    /*                                                                          */
    /* Step two: file system would try to allocate completely new group for     */
    /* single inode, if system has no requested amount of free groups, then it  */
    /* would be enough to allocate how many free group blocks we have left.     */
    /* And further allocation will proceed in the step three.                   */              
    /*                                                                          */
    /* Step three: random allocation of a data blocks                           */


    /* Step one */
    {
        struct sffs_data_block_info last_ino_info;
        errc = sffs_get_data_block_info(0, SFFS_GET_BLK_LT, &last_ino_info, ino_mem);
        if(errc < 0)
            return errc;
        
        u32_t free_spots;
        if(last_ino_info.inode_id != ino_mem->ino.i_inode_num)
            free_spots = supp_ino_blks - last_ino_info.list_id;
        else 
            free_spots = pr_inode_blks - last_ino_info.list_id;
        
        if(free_spots == 0 || inode->i_blks_count == 0)
            goto step_two;

        // Examine bitmap
        blk32_t grp_id = last_ino_info.block_id / sffs_ctx.sb.s_blocks_per_group;
        blk32_t blk_off = last_ino_info.block_id % sffs_ctx.sb.s_blocks_per_group;
        blk32_t ino_grp_bm;
        errc = __get_group_bitmap(sffs_ctx.sb.s_data_bitmap_start, grp_id, &ino_grp_bm);
        if(errc < 0)
            return errc;

        bmap_t grp_size = sffs_ctx.sb.s_blocks_per_group;
        
        /**
         *  If inode is empty, do not change location to a next entry. Whenever inode
         *  has allocated blocks, change it to point to following block bitmap value
        */
        if(ino_mem->ino.i_blks_count == 0)
            blk_off = 0;
        else 
            blk_off++;

        for(int i = blk_off; i < grp_size && allocated < alloc_blocks; i++)
        {
            if(__check_bm(&ino_grp_bm, i) != 0)
                continue;
            new_blocks[allocated] = (grp_id * grp_size) + i;
            allocated++;
        }

        if(allocated == alloc_blocks)
            goto alloc_done;
    }

step_two: 
    {
        if(sffs_ctx.sb.s_free_groups == 0)
            goto step_three;

        blk32_t blocks_per_grp = sffs_ctx.sb.s_blocks_per_group;
        u32_t grps_need = allocated / blocks_per_grp;
        if(allocated % blocks_per_grp != 0)
            grps_need++;

        for(int i = 0; i < sffs_ctx.sb.s_group_count && allocated < alloc_blocks; i++)
        {
            bmap_t curr_grp;
            errc = __get_group_bitmap(sffs_ctx.sb.s_data_bitmap_start, i, &curr_grp);
            if(errc < 0)
                return errc;
            
            if(curr_grp == 0)
            {
                for(u32_t k = 0; k < blocks_per_grp && allocated < alloc_blocks; k++)
                {
                    blk32_t block_id = (i * blocks_per_grp) + k;
                    if(__find_block(new_blocks, allocated, block_id) == false)
                    {
                        new_blocks[allocated] = block_id;
                        allocated++;
                    }
                }

                allocated_grps++;
            }
        }

        if(allocated == alloc_blocks)
            goto alloc_done;
    }

step_three:
    /**
     *  Random blocks allocation goes here. Extremely stupid algorithm.
    */
    u32_t total_blocks = sffs_ctx.sb.s_blocks_count;
    for(u32_t i = 0; i < total_blocks && allocated < alloc_blocks; i++)
    {
        if(sffs_check_data_bm(i) == 0)
        {
            if(__find_block(new_blocks, allocated, i) == false)
                new_blocks[allocated++] = i;
        }
    }

    if(allocated != alloc_blocks)
        return SFFS_ERR_FS;

alloc_done:
    // Blocks registration
    u32_t written = 0;

    struct sffs_data_block_info last_info;
    errc = sffs_get_data_block_info(0, SFFS_GET_BLK_LT, &last_info, ino_mem);
    if(errc < 0)
        return errc;

    // Write block ids to a first, potentially primary inode
    ino32_t last_ino = last_info.inode_id;
    if(last_ino == ino_mem->ino.i_inode_num)
    {
        blk32_t free_blocks = pr_inode_blks - ino_mem->ino.i_blks_count;
        u32_t pos = pr_inode_blks - free_blocks;

        while(free_blocks > 0 && written < allocated)
        {
            ino_mem->blks[pos++] = new_blocks[written];
            written++;
            free_blocks--;
        }
    }
    
    // Write down the remaining block ids
    ino32_t next_entry = last_entry;
    ino32_t next_id = last_info.list_id;
    struct sffs_inode_mem *buf;
    errc = sffs_creat_inode(0, SFFS_IFREG, 0, &buf);
    if(errc < 0)
        return errc;
    
    while(next_entry != 0 && written < allocated)
    {
        errc = sffs_read_inode(next_entry, buf);    
        if(errc < 0)
            return errc;
        
        struct sffs_inode_list *supp_ino = (struct sffs_inode_list *) buf;
        u32_t to_write;
        u32_t remaining = allocated - written;
        u32_t pos = 0;

        if(next_id != 0)
        {
            to_write = supp_ino_blks - next_id;
            pos = next_id;
        }
        else if(supp_ino_blks < remaining)
            to_write = supp_ino_blks;
        else 
            to_write = remaining;
           
        memcpy(supp_ino->blks + pos, new_blocks + written, sizeof(blk32_t) * to_write);
        errc = sffs_write_inode(buf);
        if(errc < 0)
            return errc;

        written += to_write;
        next_entry = supp_ino->i_next_entry;
    }

    ino_mem->ino.i_blks_count += allocated;
    sffs_ctx.sb.s_free_blocks_count -= allocated;
    sffs_ctx.sb.s_free_groups -= allocated_grps;

    errc = sffs_write_inode(ino_mem);
    if(errc < 0)
        return errc;

    /**
     *  At this point, all blocks which are recorded in new_blocks array are 
     *  not actually allocated but rather commited to an inode
    */
    for(u32_t i = 0; i < allocated; i++)
    {
        errc = sffs_set_data_bm(new_blocks[i]);
        
        /**
         *  If error is occured during bitmap serialization, try to unwind the ongoing
         *  process back and restore the bitmap validity
        */ 
        if(errc < 0)
        {
            for(int k = 0; k < i; k++)
            {
                sffs_err_t errc2 = sffs_unset_data_bm(new_blocks[k]);
                if(errc2 < 0)
                    return errc2; 
            }
            return errc;  
        }
    }

    free(buf);
    free(new_blocks);
    return 0;
}
