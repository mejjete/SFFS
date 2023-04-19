/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#ifndef SFFS_H
#define SFFS_H

#include <sys/types.h>
#include <stdint.h>

/**
 *  Default inode ration for SFFS is 1 : 128KB. This value
 *  is determined by size of an inode data size, that holds
 *  pointers to data blocks
*/
#define SFFS_INODE_RATIO    131072

#define SFFS_MAX_MOUNT  16

/**
 *  Limits the maximum inode size, thus, file size
 *  for a single inode
*/
#define SFFS_MAX_INODE_LIST 32

#define SFFS_MAGIC  0x53FF5346

/**
 *  SFFS differentiate between inode entry and inode itself.
 *  The inode is the sffs_inode structure. 
 *  The inode entry consists of an inode followed by a data blocks, 
 *  which size must be determined at the initialization time by
 *  __sffs_init function. But now it is a macro which represents 
 *  the predefine value for this two entries
*/
#define SFFS_INODE_SIZE             sizeof(struct sffs_inode) + 64
#define SFFS_INODE_DATA_SIZE        SFFS_INODE_SIZE
#define SFFS_INODE_ENTRY_SIZE       (SFFS_INODE_SIZE * 2)

typedef uint32_t blk32_t;       // Data block ID
typedef uint32_t ino32_t;       // Inode block ID

typedef uint8_t  u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;

/**
 *  SFFS inode. Represents a single file entiry
 * 
 *  REV. 1
*/
struct __attribute__ ((__packed__)) sffs_inode
{
    union
    {
        // For 32-bit systems
        struct 
        {
            uint32_t i_acc_time;          // Access time - low bits
            uint32_t i_acc_time_ex;       // Access time - high bits
            uint32_t i_chg_time;          // Change time - low bits
            uint32_t i_chg_time_ex;       // Change time - high bits
            uint32_t i_mod_time;          // Modification time - low bits
            uint32_t i_mod_time_ex;       // Modification time - high bits
            uint32_t i_crt_time;          // Creation time - low bits
            uint32_t i_crt_time_ex;       // Creation time - high bits
        } t32;

        // For 64-bit systems
        struct 
        {
            uint64_t i_acc_time;
            uint64_t i_chg_time;
            uint64_t i_mod_time;
            uint64_t i_crt_time;
        } t64;
    } tv;

    ino32_t i_inode_num;        // Inode number
    uint32_t i_uid_owner;       // Owner ID
    uint32_t i_gid_owner;       // Owner group ID
    uint32_t i_flags;           // File system specific flags
    uint32_t i_blks_count;      // File size in blocks
    uint16_t i_bytes_rem;       // Remainder of the size in blocks
    uint16_t i_mode;            // File type and permissions
    uint32_t i_next_entry;      // Pointer to next entry in Global Inode Table (GIT)
    uint16_t i_link_count;      // Link count

    // Align fields
    uint8_t __align1[64];         // padding
};

/**
 *  SFFS superblock resides at the header and footer 
 *  in metadata area. Holds the basic set of a file system 
 *  characteristics and state
 * 
 *  REV. 1
*/
struct __attribute__ ((__packed__)) sffs_superblock
{
    uint32_t s_inodes_count;            // Total inodes count
    uint32_t s_blocks_count;            // Total blocks count 
    uint32_t s_free_blocks_count;       // Free blocks
    uint32_t s_free_inodes_count;       // Free inodes
    uint32_t s_block_size;              // Block's size in bytes
    uint32_t s_blocks_per_group;        // Number of blocks per group
    uint16_t s_mount_time;              // Low precision mount time
    uint16_t s_write_time;              // Low precision last write time
    uint16_t s_mount_count;             // Number of active mount points
    uint16_t s_max_mount_count;         // Maximum number of active mount points
    uint16_t s_state;                   // File system state
    uint16_t s_error;                   // Last occurred error
    uint16_t s_inode_size;              // Inode size in bytes
    uint16_t s_inode_block_size;        // Inode block size in bytes
    uint32_t s_magic;                   // SFFS magic number

    uint32_t s_max_inode_list;          // Maximum value for a single inode list
    uint32_t s_features;                // File system flags
    uint32_t s_prealloc_blocks;         // How many blocks preallocate for a single file
    uint32_t s_prealloc_dir_blocks;     // How many blocks preallocate for a directory

    blk32_t s_data_bitmap_start;        // Data bitmap starting block
    blk32_t s_data_bitmap_size;         // Data bitmap size in blocks
    blk32_t s_first_data_block;         // Pointer to a first data block 

    // Global Inode Table
    blk32_t s_GIT_bitmap_start;         // Global Inode Table bitmap starting block
    blk32_t s_GIT_bitmap_size;          // Global Inode Table bitmap size in blocks
    blk32_t s_GIT_bitmap_reserved;      // Reserved blocks for GIT bitmap
    blk32_t s_GIT_start;                // Global Inode Table starting block
    blk32_t s_GIT_size;                 // Global Inode Table size in blocks
    blk32_t s_GIT_reserved;             // Reserved blocks for GIT
};

#define SFFS_SB_SIZE        sizeof(struct sffs_superblock)

/**
 *  sffs.c
*/

void __sffs_init();

/**
 *  Reads sb_id superblock, and places it in sb
*/
void sffs_read_sb(u8_t sb_id, struct sffs_superblock *sb);

#endif  // SFFS_H