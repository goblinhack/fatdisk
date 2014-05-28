/*
 * Copyright (C) 2013 Neil McGill
 *
 * See the LICENSE file.
 */

/*
 * To convert off_t to 64 bits.
 */
#define _FILE_OFFSET_BITS 64 

/*
 * Pointer sanity, high memory use
 */
#define nENABLE_PTRCHECK

/*
 * Die on errors
 */
#define nENABLE_ASSERT

/*
 * Internal sanity, very sloe
 */
#define nENABLE_TREE_SANITY

/*
 * Backtrace on error
 */
#define ENABLE_ERR_BACKTRACE

/*
 * This uses more memory but speads up disk reads.
 */
#define ENABLE_CACHING_OF_SECTORS

/*
 * FAT debugging
 */
// #define DEBUG_DIRENT_WALK
// #define DEBUG_DIRENT_REMOVE
// #define DEBUG_DIRENT_ADD
// #define DEBUG_CLUSTER_ALLOC

/*
 * How many directory chains we can handle.
 */
#define MAX_DIRENT_BLOCK                    1024

/*
 * Enough for very long dir names
 */
#define MAX_STR                             1024

/*
 * Max directory depth.
 */
#define MAX_DIR_DEPTH                       1024

#define ONE_K                               1024
#define ONE_MEG                             (1024 * 1024)
#define ONE_GIG                             (1024 * 1024 * 1024)

#define DEFAULT_DISK_SIZE                   ONE_GIG
#define DEFAULT_DISK_TYPE                   DISK_FAT32
#define DEFAULT_SECTOR_SIZE                 512

/*
 * For %PRIu etc...
 */
#define __STDC_FORMAT_MACROS

#define _GNU_SOURCE

#define CONFIG_H_INCLUDED

