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
 *  sffs_ctx.disk file descriptor. Writes absolute block 
 *  
 *  block - block id from where to read
 *  size  - number o block how many to read
 *  data  - destination for read data
 * 
 *  Caller is responsible for allocating enough space 
 *  for data to be read
*/
int sffs_read_blk(blk32_t block, void *data, size_t size);

/**
 *  The same as sffs_write_blk but writes relative blocks to a 
 *  data blocks region
*/
int sffs_write_data_blk(blk32_t block, void *data, size_t blks);

/**
 *  The same as sffs_read_blk but reads relative blocks from a
 *  data blocks region
*/
int sffs_read_data_blk(blk32_t block, void *data, size_t blks);

#endif  // SFFS_DEVICE_H