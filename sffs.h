/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#ifndef SFFS_H
#define SFFS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdbool.h>

/**
 *  Default inode ration for SFFS is 1 : 128KB. This value is determined 
 *  by inode data size, which holds pointers to data blocks
*/
#define SFFS_INODE_RATIO    131072

#define SFFS_MAX_MOUNT  16

#ifndef SFFS_MAX_INODE_LIST
/**
 *  This number is the maximum number of inode list that one single 
 *  file can support. Limits the maximum file size. Particularly this
 *  value is defined for a debugging purposes.
*/
#define SFFS_MAX_INODE_LIST 32
#endif 

#define SFFS_MAGIC  0x53FF5346

/**
 *  SFFS file permission flags
*/
#define	SFFS_IRUSR  04000	// Read by owner
#define	SFFS_IWUSR	0200    // Write by owner
#define	SFFS_IXUSR	0100	// Execute by owner

/**
 *  Mixed file permission flags
*/
#define	SFFS_IRWXU	(SFFS_IRUSR | SFFS_IWUSR | SFFS_IXUSR)

#define	SFFS_IRGRP	(S_IRUSR >> 3)	    // Read by group
#define	SFFS_IWGRP	(S_IWUSR >> 3)	    // Write by group
#define	SFFS_IXGRP	(S_IXUSR >> 3)	    // Execute by group

// Read, write, and execute by group
#define	SFFS_IRWXG	(S_IRWXU >> 3)

#define	SFFS_IROTH	(S_IRGRP >> 3)	    // Read by others
#define	SFFS_IWOTH	(S_IWGRP >> 3)	    // Write by others
#define	SFFS_IXOTH	(S_IXGRP >> 3)	    // Execute by others

// Read, write, and execute by others
#define	SFFS_IRWXO	(S_IRWXG >> 3)

/**
 *  SFFS file types
*/
#define	SFFS_IFDIR	0040000	    // Directory
#define	SFFS_IFCHR	0020000	    // Character device
#define	SFFS_IFBLK	0060000	    // Block device
#define	SFFS_IFREG	0100000	    // Regular file
#define	SFFS_IFIFO	0010000	    // FIFO
#define	SFFS_IFLNK	0120000	    // Symbolic link
#define	SFFS_IFSOCK	0140000	    // Socket

#define	SFFS_IFMT	0170000	    // File type bits in i_mode

/**
 *  SFFS file type checker
*/
#define	SFFS_ISTYPE(mode, mask)	(((mode) & SFFS_IFMT) == (mask))

#define	SFFS_ISDIR(mode)	SFFS_ISTYPE((mode), __S_IFDIR)
#define	SFFS_ISCHR(mode)	SFFS_ISTYPE((mode), __S_IFCHR)
#define	SFFS_ISBLK(mode)	SFFS_ISTYPE((mode), __S_IFBLK)
#define	SFFS_ISREG(mode)	SFFS_ISTYPE((mode), __S_IFREG)
#define SfFS_ISFIFO(mode)   SFFS_ISTYPE((mode), __S_IFIFO)
#define SFFS_ISLNK(mode)    SFFS_ISTYPE((mode), __S_IFLNK)

/**
 *  Special flags for controlling allocation.
 *  SFFS_GRP_SEQ - used to indicate that inode's group block consists
 *  only of their own blocks, thus, group blocks associated with this 
 *  inode not interwined with blocks relating to other inodes
 *  SFFS_GRP_NOSEQ - used to indicate the oposite, where the inode's
 *  group blocks is non-holistic and contains blocks that relates
 *  to other inodes.
*/
#define SFFS_GRP_SEQ        0000001
#define SFFS_GRP_NOSEQ      0000002

/**
 *  SFFS differentiate between inode entry and inode itself.
 *  The inode is the sffs_inode structure. 
 *  The inode entry consists of an inode followed by a data blocks, 
 *  which size must be determined at the initialization time by
 *  __sffs_init function. But now it is a macro which represents 
 *  the predefine values for this two entries
*/
#define SFFS_INODE_SIZE             sizeof(struct sffs_inode)
#define SFFS_INODE_DATA_SIZE        SFFS_INODE_SIZE

#define SFFS_RESV_INODES    0

typedef uint32_t blk32_t;       // Data block ID
typedef uint32_t ino32_t;       // Inode ID
typedef uint32_t bmap_t;        // Bitmap ID

typedef uint8_t  u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;

typedef enum
{
    /**
     *  This default values are used for handlers to indicate there's no 
     *  errors, as well as for bitmaps to indicates whether slots are
     *  occupied or not. Thus 1 and 0 are reserved for file system needs.
    */
    __DEFAULT1 = 1,             
    __DEFAULT2 = 0,
    
    /**
     *  Basic error handler codes
    */
    SFFS_ERR_INVARG = -1,       // Invalid arguments passed to a handler
    SFFS_ERR_INVBLK = -2,       // Invalid block size
    SFFS_ERR_INIT,              // Common error occured during mounting
    SFFS_ERR_MEMALLOC,          // Cannot allocate memory       
    SFFS_ERR_FS,                // File system structure is corrupted
    SFFS_ERR_NOSPC,             // No free space

    /**
     *  Device error codes
    */
    SFFS_ERR_DEV_WRITE,         // Device write operation error
    SFFS_ERR_DEV_READ,          // Device read operation error
    SFFS_ERR_DEV_SEEK,          // Device seek operation error
    SFFS_ERR_DEV_STAT,          // Device stat or statfs error
}sffs_err_t;

/**
 *  On-disk representation of an SFFS inode. 
 *  Represents a single file entry
 * 
 *  REV. 1
*/
struct __attribute__ ((__packed__)) sffs_inode
{
    ino32_t i_inode_num;        // Inode number
    uint32_t i_next_entry;      // Pointer to next entry in Global Inode Table (GIT)
    uint32_t i_uid_owner;       // Owner ID
    uint32_t i_gid_owner;       // Owner group ID
    uint32_t i_flags;           // File system specific flags
    uint32_t i_blks_count;      // File size in blocks
    uint16_t i_bytes_rem;       // Remainder of the size in blocks
    uint16_t i_mode;            // File type and permissions
    uint16_t i_link_count;      // Link count
    uint16_t i_grp_info;        // Inode group's flags

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

    uint8_t __align1[64];       // padding (reserved for future use)
};

/**
 *  In-memory representation of an SFFS inode
 * 
 *  Rev. 1
*/
struct __attribute__ ((__packed__)) sffs_inode_mem 
{
    struct sffs_inode ino;      // Inode instance
    uint32_t size;              // Size in bytes of this record
    blk32_t blk[];              // Direct data blocks of inode
};

/**
 *  SFFS supports list of inodes that accumulate data blocks. List of inodes
 *  consist of a primary inode, that holds struct sffs_inode and number of 
 *  supplementary inodes that holds sffs_inode_list structure at the header
 *  and sequential number of data blocks corresponding to primary inode.
 * 
 *  Rev. 1
*/
struct __attribute__ ((__packed__)) sffs_inode_list
{
    u32_t i_inode_num;
    u32_t i_next_entry;
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
    uint32_t s_inodes_reserved;         // Number of reserved inodes
    uint32_t s_blocks_count;            // Total data blocks count
    uint32_t s_free_blocks_count;       // Free blocks
    uint32_t s_free_inodes_count;       // Free inodes
    uint32_t s_block_size;              // Block's size in bytes
    uint32_t s_blocks_per_group;        // Number of blocks per group
    uint32_t s_group_count;             // Number of group blocks
    uint32_t s_free_groups;             // Number of free group blockss
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
    uint32_t s_prealloc_blocks;         // How many blocks preallocate for a file
    uint32_t s_prealloc_dir_blocks;     // How many blocks preallocate for a directory

    blk32_t s_data_bitmap_start;        // Data bitmap starting block
    blk32_t s_data_bitmap_size;         // Data bitmap size in blocks
    blk32_t s_first_data_block;         // Pointer to a first data block 

    // Global Inode Table
    blk32_t s_GIT_bitmap_start;         // Global Inode Table bitmap starting block
    blk32_t s_GIT_bitmap_size;          // Global Inode Table bitmap size in blocks
    blk32_t s_GIT_start;                // Global Inode Table starting block
    blk32_t s_GIT_size;                 // Global Inode Table size in blocks
};

#define SFFS_SB_SIZE        sizeof(struct sffs_superblock)

/*      sffs.c      */

/**
 *  Do basic initialization stuff, called during mounting.
 * 
 *  If handler fails, the error code is returned
*/
sffs_err_t __sffs_init();

/**
 *  The SFFS manages two superblocks. This allows for a file system 
 *  to store crucial data within two places that increases its viability.
 *  Although this API handler supports superblock id (sb_id), at the time
 *  there's no second superblock location either as no second copy of superblock. 
 *  I wish it would be added soon.
 * 
 *  If handler fails, the error code is returned
*/
sffs_err_t sffs_read_sb(u8_t sb_id, struct sffs_superblock *sb);

/**
 *  Writes superblock pointed by sb to a specified location.
 *  The reason for sb_id look sffs_read_sb.
*/
sffs_err_t sffs_write_sb(u8_t sb_id, struct sffs_superblock *sb);

/**
 *  Creates and initializes new inode instance in inode.
 *  This handler performs check on the mode and flags arguments. 
 *  ino_id is not checked.
 * 
 *  If handler fails, the error code is returned
*/
sffs_err_t sffs_creat_inode(ino32_t ino_id, mode_t mode, int flags,
    struct sffs_inode_mem **ino_mem);

/**
 *  Serializes inode to a disk in location placed in inode itself.
 * 
 *  If handler fails, the error code is returned
*/
sffs_err_t sffs_write_inode(struct sffs_inode_mem *inode); 

/**
 *  Reads inode by a given inode identificator (ino_id) and puts
 *  it into inode. If blocks pointer is not a NULL, it copies
 *  the sequential 
 * 
 *  If handler fails, the error code is returned
*/
sffs_err_t sffs_read_inode(ino32_t ino_id, struct sffs_inode_mem *inode);
 
/**
 *  Updates current inode
*/
sffs_err_t sffs_update_inode(struct sffs_inode_mem *old_inode, struct sffs_inode_mem *new_inode);

/**
 *  Put inode data block at blk_id to result
*/
sffs_err_t sffs_get_inode_blk(ino32_t ino_id, blk32_t blk_id, blk32_t *result);

/**
 *  Extremely stupid implementation of inode allocation algorithm.
 *  It does not implies on-disk layout not trying to effectively
 *  place inode. It just sequentially scans GIT bitmap and chooses the 
 *  first free inode position. Additionally, it does not consider mode 
 *  argument as well. Tend to be rewritten further.
 * 
 *  If handler fails, the error code is returned
*/
sffs_err_t sffs_alloc_inode(ino32_t *ino_id, mode_t mode);

/**
 *  Allocates blk_count data blocks for inode. Appends newly 
 *  allocated blocks to inode.
*/
sffs_err_t sffs_alloc_data(size_t blk_count, struct sffs_inode *inode);

/*      bitmaps.c       */

/**
 *  Bitmap handlers for Data Blocks.
 *  If bitmap handlers fails, the error code is returned
*/
sffs_err_t sffs_set_data_bm(bmap_t);
sffs_err_t sffs_unset_data_bm(bmap_t);
sffs_err_t sffs_check_data_bm(bmap_t);

/**
 *  Bitmap handlers for Global Inode Table.
 *  If bitmap handlers fails, the error code is returned
*/
sffs_err_t sffs_set_GIT_bm(bmap_t);
sffs_err_t sffs_unset_GIT_bm(bmap_t);
sffs_err_t sffs_check_GIT_bm(bmap_t);

#endif  // SFFS_H