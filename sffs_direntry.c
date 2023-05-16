#include "sffs.h"
#include "sffs_device.h"
#include <stdlib.h>
#include <string.h>

sffs_err_t sffs_creat_direntry(struct sffs_inode_mem *parent, struct sffs_inode_mem *child)
{
    if(!child)
        return SFFS_ERR_INVARG;
    
    // Root has no parent, so make it to point to iteself
    if(!parent)
        parent = child;

    // Alloc maximum size that can carry "." and ".." entries
    struct sffs_direntry *def_dir = malloc(SFFS_DIRENTRY_LENGTH + 3);
    if(!def_dir)
        return SFFS_ERR_MEMALLOC;
    
    if(child->ino.i_blks_count != 0)
        return SFFS_ERR_INVARG;
    
    sffs_err_t errc;
    errc = sffs_alloc_data_blocks(1, child);
    if(errc < 0)
        return errc;
    
    struct sffs_data_block_info db_info;
    errc = sffs_get_data_block_info(0, SFFS_GET_BLK_LT, &db_info, child);
    if(errc < 0)
        return errc;

    blk32_t block = db_info.block_id;
    blk32_t block_size = sffs_ctx.sb.s_block_size;
    u32_t accum_rec = 0;
    char ch;

    // Creat "." entry
    def_dir->ino_id = child->ino.i_inode_num;
    def_dir->file_type = SFFS_DIRENTRY_MODE(child->ino.i_mode);
    def_dir->rec_len = SFFS_DIRENTRY_LENGTH + 1;
    ch = '.';
    memcpy(def_dir->name, &ch, 1);
    memcpy(sffs_ctx.cache, def_dir, def_dir->rec_len);
    accum_rec += def_dir->rec_len;

    // Creat ".." entry
    def_dir->ino_id = parent->ino.i_inode_num;
    def_dir->file_type = SFFS_DIRENTRY_MODE(child->ino.i_mode);
    def_dir->rec_len = SFFS_DIRENTRY_LENGTH + 2;
    memcpy(def_dir->name, "..", 2);
    memcpy(sffs_ctx.cache + accum_rec, def_dir, def_dir->rec_len);
    accum_rec += def_dir->rec_len; 

    // Creat last terminating entry
    def_dir->ino_id = 0;
    def_dir->file_type = 0;
    def_dir->rec_len = block_size - accum_rec;
    memcpy(sffs_ctx.cache + accum_rec, def_dir, SFFS_DIRENTRY_LENGTH);

    errc = sffs_write_blk(block, sffs_ctx.cache, 1);
    if(errc < 0)
        return errc;
    return 0;
}