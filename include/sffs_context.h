/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#ifndef SFFS_CONTEXT_H
#define SFFS_CONTEXT_H

#include <sffs.h>

struct sffs_options
{
    int fs_size;
    const char *log_file;
};

#define SFFS_OPT_INIT(t, p) { t, offsetof(struct sffs_options, p), 1 }

typedef struct sffs_context
{
    int disk_id;                // Image file descriptor
    int log_id;                 // Log file descriptor
    blk32_t block_size;         // Block size for quick access
    struct sffs_superblock sb;  // Super block instance
    void *cache;                // Private data
} sffs_context_t;

// sffs.c
extern sffs_context_t sffs_ctx;

#endif // SFFS_CONTEXT_H