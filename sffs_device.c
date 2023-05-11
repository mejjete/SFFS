/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#include "sffs_device.h"

int sffs_write_blk(blk32_t block, void *data, size_t size)
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
    uint64_t ssize = size;
    uint64_t bytes = ssize * sffs_ctx.sb.s_block_size;

    int seek;
    if((seek = lseek64(sffs_ctx.disk_id, offset, SEEK_SET)) < 0)
        return seek;

    int ret = write(sffs_ctx.disk_id, data, bytes);
    if(ret < 0)
        return ret;

    // fsync() should be remove as soon as cache will be added
    fsync(sffs_ctx.disk_id);
    return ret;
};

int sffs_read_blk(blk32_t block, void *data, size_t size)
{
    if(!data)
        return -1;
    
    uint64_t blk = block;
    uint64_t offset = blk * sffs_ctx.sb.s_block_size;
    uint64_t ssize = size;
    uint64_t bytes = ssize * sffs_ctx.sb.s_block_size;

    int seek;
    if((seek = lseek64(sffs_ctx.disk_id, offset, SEEK_SET)) < 0)
        return seek;

    return read(sffs_ctx.disk_id, data, bytes);
};