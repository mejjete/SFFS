/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#include "sffs.h"
#include "sffs_context.h"
#include "sffs_device.h"

static sffs_err_t __sffs_set_bm(blk32_t, bmap_t, u8_t);
static sffs_err_t __sffs_check_bm(blk32_t, bmap_t);
static sffs_err_t __set_bm(blk32_t *, bmap_t, u8_t);
static sffs_err_t __check_bm(blk32_t *, bmap_t);

sffs_err_t sffs_set_data_bm(bmap_t id)
{ 
    return __sffs_set_bm(sffs_ctx.sb.s_GIT_bitmap_start, id, 1); 
}

sffs_err_t sffs_set_GIT_bm(bmap_t id)
{
    return __sffs_set_bm(sffs_ctx.sb.s_GIT_bitmap_start, id, 1);
}

sffs_err_t sffs_unset_data_bm(bmap_t id)
{
    return __sffs_set_bm(sffs_ctx.sb.s_data_bitmap_start, id, 0);
}

sffs_err_t sffs_unset_GIT_bm(bmap_t id)
{
    return __sffs_set_bm(sffs_ctx.sb.s_GIT_bitmap_start, id, 0);
}

sffs_err_t sffs_check_data_bm(bmap_t id)
{
    return __sffs_check_bm(sffs_ctx.sb.s_data_bitmap_start, id);
}

sffs_err_t sffs_check_GIT_bm(bmap_t id)
{
    return __sffs_check_bm(sffs_ctx.sb.s_GIT_bitmap_start, id);
}

static sffs_err_t __sffs_set_bm(blk32_t bm, bmap_t id, u8_t value)
{
    value &= 0x1;
    blk32_t block_size = sffs_ctx.sb.s_block_size;
    blk32_t bm_start = bm;
    blk32_t bm_block = id / block_size;     // Block number that holds id bitmap value
    bmap_t bm_id = id % block_size;         // Bit number wihtin victim block

    sffs_err_t errc;
    errc = sffs_read_blk(bm_start + bm_block, sffs_ctx.cache, 1);
    if(errc < 0)
        return errc;

    errc = __set_bm(sffs_ctx.cache, bm_id, value);
    if(errc < 0)
        return errc;
    
    errc = sffs_write_blk(bm_start + bm_block, sffs_ctx.cache, 1);
    if(errc < 0)
        return errc;
    return true;
}

sffs_err_t __sffs_check_bm(blk32_t bm, bmap_t id)
{
    blk32_t block_size = sffs_ctx.sb.s_block_size;
    blk32_t bm_start = bm;
    blk32_t bm_block = id / block_size;     // Block number that holds id bitmap value
    bmap_t bm_id = id % block_size;         // Bit number wihtin victim block

    sffs_err_t errc;
    errc = sffs_read_blk(bm_start + bm_block, sffs_ctx.cache, 1);
    if(errc < 0)
        return errc;

    return __check_bm(sffs_ctx.cache, bm_id);
}

static sffs_err_t __check_bm(blk32_t *bm, bmap_t id)
{
    u32_t byte_id = id / 8;
    u32_t bit_id = id % 8;
    u8_t *byte = (u8_t *)(bm) + byte_id;
    if((*byte & (1 << bit_id)) == (1 << bit_id))
        return true;
    else 
        return false;
}

static sffs_err_t __set_bm(blk32_t *bm, bmap_t id, u8_t value)
{   
    u32_t byte_id = id / 8;
    u32_t bit_id = id % 8;
    u8_t *byte = (u8_t *)(bm) + byte_id;

    if((*byte & (1 << bit_id)) != (1 << bit_id))
    {
        if(value == 1)
            *byte |= (1 << bit_id);     // set bit to 1
        else if(value == 0)
        {
            u8_t mask = ~(1 << bit_id);
            *byte &= mask;                  // set bit to 0
        }
    }
    else 
        return SFFS_ERR_FS;

    return true;
}