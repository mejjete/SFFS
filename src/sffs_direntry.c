/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#include <sffs.h>
#include <sffs_device.h>
#include <stdlib.h>
#include <string.h>

sffs_err_t sffs_init_direntry(sffs_context_t *sffs_ctx, struct sffs_inode_mem *parent, 
    struct sffs_inode_mem *child)
{
    if(!child)
        return SFFS_ERR_INVARG;

    // Root has no parent, so make it to point to itself
    if(!parent)
        parent = child;
    
    if(!SFFS_ISDIR(parent->ino.i_mode))
        return SFFS_ERR_INVARG;

    // Alloc maximum size that can carry "." and ".." entries
    struct sffs_direntry *def_dir = malloc(SFFS_DIRENTRY_LENGTH + 3);
    if(!def_dir)
        return SFFS_ERR_MEMALLOC;

    if(child->ino.i_blks_count != 0)
        return SFFS_ERR_INVARG;
    
    sffs_err_t errc;
    errc = sffs_alloc_data_blocks(sffs_ctx, 1, child);
    if(errc < 0)
        return errc;
    
    struct sffs_data_block_info db_info;
    errc = sffs_get_data_block_info(sffs_ctx, 0, SFFS_GET_BLK_LT, &db_info, child);
    if(errc < 0)
        return errc;

    blk32_t block = db_info.block_id;
    blk32_t block_size = sffs_ctx->sb.s_block_size;
    u32_t accum_rec = 0;
    char ch;

    // Creat "." entry
    def_dir->ino_id = child->ino.i_inode_num;
    def_dir->file_type = SFFS_DIRENTRY_MODE(child->ino.i_mode);
    def_dir->rec_len = SFFS_DIRENTRY_LENGTH + 1;
    ch = '.';
    memcpy(def_dir->name, &ch, 1);
    memcpy(sffs_ctx->cache, def_dir, def_dir->rec_len);
    accum_rec += def_dir->rec_len;

    // Creat ".." entry
    def_dir->ino_id = parent->ino.i_inode_num;
    def_dir->file_type = SFFS_DIRENTRY_MODE(child->ino.i_mode);
    def_dir->rec_len = SFFS_DIRENTRY_LENGTH + 2;
    memcpy(def_dir->name, "..", 2);
    memcpy(sffs_ctx->cache + accum_rec, def_dir, def_dir->rec_len);
    accum_rec += def_dir->rec_len; 

    // Creat last terminating entry
    def_dir->ino_id = 0;
    def_dir->file_type = 0;
    def_dir->rec_len = block_size - accum_rec;
    memcpy(sffs_ctx->cache + accum_rec, def_dir, SFFS_DIRENTRY_LENGTH);

    errc = sffs_write_data_blk(sffs_ctx, block, sffs_ctx->cache, 1);
    if(errc < 0)
        return errc;
    return 0;
}

sffs_err_t sffs_new_direntry(sffs_context_t *sffs_ctx, struct sffs_inode *inode, 
    const char *entry, struct sffs_direntry **dir)
{
    if(!inode || !entry)
        return SFFS_ERR_INVARG;
    
    if(!SFFS_ISDIR(inode->i_mode))
        return SFFS_ERR_INVARG;

    size_t path_len = strlen(entry);
    if(path_len + SFFS_DIRENTRY_LENGTH > SFFS_MAX_DIR_ENTRY)
        return SFFS_ERR_INVARG;

    u16_t type = SFFS_DIRENTRY_MODE(inode->i_mode);
    u16_t rec_len = SFFS_DIRENTRY_LENGTH + path_len;
    ino32_t ino = inode->i_inode_num;

    *dir = (struct sffs_direntry *) malloc(rec_len);
    if(!dir)
        return SFFS_ERR_MEMALLOC;

    struct sffs_direntry *d = *dir;

    d->file_type = type;
    d->ino_id = ino;
    d->rec_len = rec_len;
    memcpy(d->name, entry, path_len); 
    return 0;
}

sffs_err_t  sffs_lookup_direntry(sffs_context_t *sffs_ctx, struct sffs_inode_mem *parent, 
    const char *path, struct sffs_direntry **direntry, struct sffs_data_block_info *info)
{
    if(!parent || !path)
        return SFFS_ERR_INVARG;
    
    if(!SFFS_ISDIR(parent->ino.i_mode))
        return SFFS_ERR_INVARG;
    
    sffs_err_t errc;
    struct sffs_data_block_info db_info;
    db_info.content = malloc(sffs_ctx->sb.s_block_size);
    if(!db_info.content)
        return SFFS_ERR_MEMALLOC;

    u32_t ino_blocks = parent->ino.i_blks_count;
    u16_t accum_rec = 0;
    u16_t rec_len;
    struct sffs_direntry *buf = (struct sffs_direntry *) 
        malloc(SFFS_MAX_DIR_ENTRY + 1);
    
    if(!buf)
        return SFFS_ERR_MEMALLOC;

    bool exist = 0;
    for(u32_t i = 0; i < ino_blocks; i++)
    {
        int flags = SFFS_GET_BLK_RD;
        errc = sffs_get_data_block_info(sffs_ctx, i, flags, &db_info, parent);
        if(errc < 0)
            return errc;
        
        u8_t *dptr = (u8_t *) db_info.content;
        accum_rec = 0;

        do 
        {
            struct sffs_direntry *temp = (struct sffs_direntry *) dptr;
            rec_len = temp->rec_len;

            memcpy(buf, temp, rec_len);
            size_t name_len = rec_len - SFFS_DIRENTRY_LENGTH;
            buf->name[name_len] = 0;
            
            if(strcmp(buf->name, path) == 0)
            {
                exist = true;
                break;    
            }

            accum_rec += rec_len;
            dptr += rec_len;
        } while(accum_rec < sffs_ctx->sb.s_block_size);

        if(exist == true)
            break;
    }

    /**
     *  If user requested directory info either, then fill up struct sffs_data_block_info
     *  in the following way:
     *  block_id    - inode's block id that contains path
     *  list_id     - offset of the direntry within the block_id block
     *  inode_id    - parent's inode (always the parent's inode number)
     *  flags       - not used now
     *  content     - NULL
    */
    if(exist && info != NULL)
    {
        info->block_id = db_info.block_id;
        info->flags = db_info.flags;
        info->list_id = accum_rec;
        info->inode_id = parent->ino.i_inode_num;
        info->content = NULL;
    }

    if(direntry != NULL)
        *direntry = buf;
    else 
        free(buf);

    free(db_info.content);
    return exist;
}

sffs_err_t sffs_add_direntry(sffs_context_t *sffs_ctx, struct sffs_inode_mem *parent, 
    struct sffs_direntry *direntry)
{
    if(!parent || !direntry)
        return SFFS_ERR_INVARG;
    
    if(direntry->rec_len > SFFS_MAX_DIR_ENTRY)
        return SFFS_ERR_INVARG;

    sffs_err_t errc;
    struct sffs_data_block_info db_info;
    db_info.content = malloc(sffs_ctx->sb.s_block_size);
    if(!db_info.content)
        return SFFS_ERR_MEMALLOC;

    /**
     *  Typical directory entry would occupy from 1 to couple of blocks,
     *  so it seems quite resonable to try to find empty gap (left after
     *  deletion) within directory blocks instead of just allocating new one  
    */
    u32_t ino_blocks = parent->ino.i_blks_count;
    u16_t accum_rec = 0;
    u16_t rec_len;
    struct sffs_direntry *d;

    /**
     *  SFFS does not allow duplicate elements in directory
    */
    errc = sffs_lookup_direntry(sffs_ctx, parent, direntry->name, NULL, NULL);
    if(errc == 1)
        return SFFS_ERR_ENTEXIS;
    else if(errc < 0)
        return errc;

    for(u32_t i = 0; i < ino_blocks; i++)
    {
        int flags = SFFS_GET_BLK_RD;
        errc = sffs_get_data_block_info(sffs_ctx, i, flags, &db_info, parent);
        if(errc < 0)
            return errc;
        
        u8_t *dptr = (u8_t *) db_info.content;
        accum_rec = 0;

        do 
        {
            d = (struct sffs_direntry *) dptr;
            rec_len = d->rec_len;
            dptr += rec_len;

            if(rec_len >= direntry->rec_len && d->ino_id == 0)
                break;
            
            accum_rec += rec_len;
        } while(accum_rec < sffs_ctx->sb.s_block_size);
    }

    /**
     *  If we couldn't find big enough free space to put new directory in within the current 
     *  directory blocks, then we have to allocate new block, initialize its first
     *  free directory entries and proceed with allocation
    */
    struct sffs_direntry *buf = (struct sffs_direntry *) malloc(SFFS_DIRENTRY_LENGTH);
    if(!buf)
        return SFFS_ERR_MEMALLOC;

    if(d->rec_len < direntry->rec_len || d->rec_len == sffs_ctx->sb.s_block_size)
    {
        errc = sffs_alloc_data_blocks(sffs_ctx, 1, parent);
        if(errc < 0)
            return errc;
        
        // Get the last allocated block
        int flags = SFFS_GET_BLK_LT | SFFS_GET_BLK_RD;
        errc = sffs_get_data_block_info(sffs_ctx, 0, flags, &db_info, parent);
        if(errc < 0)
            return errc;
        
        buf->file_type = 0;
        buf->ino_id = 0;
        buf->rec_len = sffs_ctx->sb.s_block_size;
        accum_rec = 0;

        memcpy(db_info.content, buf, SFFS_DIRENTRY_LENGTH);
        errc = sffs_write_data_blk(sffs_ctx, db_info.block_id, db_info.content, 1);
        if(errc < 0)
        {
            free(buf);
            return errc;
        }
    }

    /**
     *  Add new direntry
    */
    u8_t *data = (u8_t *) db_info.content; 
    memcpy(data + accum_rec, direntry, direntry->rec_len);
    accum_rec += direntry->rec_len;

    buf->file_type = 0;
    buf->ino_id = 0;
    buf->rec_len = sffs_ctx->sb.s_block_size - accum_rec;
    memcpy(data + accum_rec, buf, SFFS_DIRENTRY_LENGTH);

    errc = sffs_write_data_blk(sffs_ctx, db_info.block_id, db_info.content, 1);
    if(errc < 0)
        return errc;

    free(buf);
    free(db_info.content);
    return 0;
}