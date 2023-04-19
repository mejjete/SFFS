/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#ifndef SFFS_DEVICE_H
#define SFFS_DEVICE_H

#include <unistd.h>
#include "sffs_context.h"

/**
 *  device.c
*/

/**
 *  Device write operation.
 *  Issues write to underlying device denoted by global
 *  sffs_ctx.disk_id file descriptor. 
 *  
 *  block - block id where to write
 *  size  - number of blocks how many to write
 *  data  pointer to data what to write
 * 
 *  Caller is responsible for allocating enough space
 *  for data to be written 
*/
int sffs_write_blk(blk32_t block, void *data, size_t size);

/**
 *  Device read operation.
 *  Issues read to underlying device denoted by global 
 *  sffs_ctx.disk file descriptor 
 *  
 *  block - block id from where to read
 *  size  - number o block how many to read
 *  data  - destination for read data
 * 
 *  Caller is responsible for allocating enough space 
 *  for data to be read
*/
int sffs_write_blk(blk32_t block, void *data, size_t size);

/**
 *  
*/
int sffs_update_blk(blk32_t block, size_t size);

/**
 *  Device read operation.
 *  Unlike the sffs_op_blk operation, this one
 *  reads up to size bytes (not blocks) at the specified
 *  block
 * 
 *  Caller is responsible for allocating enough space 
 *  for data to be written
*/
int sffs_write_bs(blk32_t block, void *data, size_t size)

#endif  // SFFS_DEVICE_H