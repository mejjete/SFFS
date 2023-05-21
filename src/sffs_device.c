/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#include <sffs_device.h>

int sffs_write_blk(blk32_t block, void *data, size_t blks)
{
    /**
     *  We cannot write to a 0 block, its allocated 
     *  for a boot region
    */
    if(block == 0)
        return -1;
    
    if(!data)
        return -1;
    
    uint64_t blk = block;
    uint64_t offset = blk * sffs_ctx.sb.s_block_size;
    uint64_t ssize = blks;
    uint64_t bytes = ssize * sffs_ctx.sb.s_block_size;

    int seek;
    if((seek = lseek64(sffs_ctx.disk_id, offset, SEEK_SET)) < 0)
        return seek;

    int wr = write(sffs_ctx.disk_id, data, bytes);
    if(wr < 0)
        return wr;

    // fsync() should be removed as soon as cache will be added
    int temp = fsync(sffs_ctx.disk_id); 
    if(temp < 0)
        return temp;
    return wr;
}

int sffs_read_blk(blk32_t block, void *data, size_t blks)
{
    if(!data)
        return -1;
    
    uint64_t blk = block;
    uint64_t offset = blk * sffs_ctx.sb.s_block_size;
    uint64_t ssize = blks;
    uint64_t bytes = ssize * sffs_ctx.sb.s_block_size;

    int seek;
    if((seek = lseek64(sffs_ctx.disk_id, offset, SEEK_SET)) < 0)
        return seek;

    return read(sffs_ctx.disk_id, data, bytes);
}

int sffs_write_data_blk(blk32_t block, void *data, size_t blks)
{    
    if(!data)
        return -1;
    
    blk32_t data_start = sffs_ctx.sb.s_GIT_bitmap_size + sffs_ctx.sb.s_GIT_size
        + sffs_ctx.sb.s_data_bitmap_size;
    
    // Include boot region as well
    if(sffs_ctx.sb.s_block_size <= 1024)
        data_start += 1024 / sffs_ctx.sb.s_block_size;

    uint64_t blk = data_start + block;
    uint64_t offset = blk * sffs_ctx.sb.s_block_size;
    uint64_t ssize = blks;
    uint64_t bytes = ssize * sffs_ctx.sb.s_block_size;

    int seek;
    if((seek = lseek64(sffs_ctx.disk_id, offset, SEEK_SET)) < 0)
        return seek;

    int wr = write(sffs_ctx.disk_id, data, bytes);
    if(wr < 0)
        return wr;

    // fsync() should be removed as soon as cache will be added
    int temp = fsync(sffs_ctx.disk_id); 
    if(temp < 0)
        return temp;
    return wr;
}

int sffs_read_data_blk(blk32_t block, void *data, size_t blks)
{
    if(!data)
        return -1;
    
    blk32_t data_start = sffs_ctx.sb.s_GIT_bitmap_size + sffs_ctx.sb.s_GIT_size
        + sffs_ctx.sb.s_data_bitmap_size;
    
    // Include boot region as well
    if(sffs_ctx.sb.s_block_size <= 1024)
        data_start += 1024 / sffs_ctx.sb.s_block_size;

    uint64_t blk = data_start + block;
    uint64_t offset = blk * sffs_ctx.sb.s_block_size;
    uint64_t ssize = blks;
    uint64_t bytes = ssize * sffs_ctx.sb.s_block_size;

    int seek;
    if((seek = lseek64(sffs_ctx.disk_id, offset, SEEK_SET)) < 0)
        return seek;
    
    return read(sffs_ctx.disk_id, data, bytes);
}
