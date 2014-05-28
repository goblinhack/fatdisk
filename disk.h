/*
 * Copyright (C) 2013 Neil McGill
 *
 * See the LICENSE file for license.
 */

/*
 * Boot record and partition structures
 *
 * Disk layout:
 *
 * 0x000 - 0x1BDH Reserved
 * 0x1BE - 0x1CDH part #1 descriptor
 * 0x1CE - 0x1DDH part #2 descriptor
 * 0x1DE - 0x1EDH part #3 descriptor
 * 0x1EE - 0x1FDH part #4 descriptor
 * 0x1FE - 0x1FFH Signature word (AA55H)
 *
 * No support for extended partitions yet
 */
#define MAX_PARTITON		            4

#define SECTOR(s)                           ((s) & 0x3f)
#define CYLINDER(s, c)                      ((c) | (((s) & 0xc0) << 2))

/*
 * For verbose output
 */
static const int32_t OUTPUT_FORMAT_WIDTH    = 32;

/*
 * Disk constants
 */
static const uint32_t PART_BASE             = 0x1BE;

/*
 * Boot record with FAT16 extension.
 */
typedef struct {
    uint8_t     bios_drive_num;
    uint8_t     reserved1;
    uint8_t     boot_signature;
    uint32_t    volume_id;
    uint8_t     volume_label[11];
    uint8_t     fat_type_label[8];
} __attribute__ ((packed)) boot_record_fat16_t;

/*
 * Boot record with FAT32 extension.
 */
typedef struct {
    /*
     * The number of blocks occupied by one copy of the File Allocation Table.
     */
    uint32_t    fat_size_sectors;
    uint16_t    extended_flags;
    uint16_t    fat_version;
    uint32_t    root_cluster;
    uint16_t    fat_info;
    uint16_t    backup_boot_sector;
    uint8_t     reserved_0[12];
    uint8_t     drive_number;
    uint8_t     reserved_1;
    uint8_t     boot_signature;
    uint32_t    volume_id;
    uint8_t     volume_label[11];
    uint8_t     fat_type_label[8];
} __attribute__ ((packed)) boot_record_fat32_t;

/*
 * Boot record.
 */
typedef struct {
    uint8_t     bootjmp[3];
    uint8_t     oem_id[8];
    uint16_t    sector_size;
    uint8_t     sectors_per_cluster;
    uint16_t    reserved_sector_count;
    uint8_t     number_of_fats;
    uint16_t    number_of_dirents;

    /*
     * If 0, look in sector_count_large
     */
    uint16_t    sector_count;
    uint8_t     media_type;

    /*
     * Set for FAT12/16 only.
     *
     * The number of blocks occupied by one copy of the File Allocation Table.
     */
    uint16_t    fat_size_sectors;
    uint16_t    sectors_per_track;
    uint16_t    nheads;
    uint32_t    sectors_hidden;
    uint32_t    sector_count_large;

    union {
        boot_record_fat32_t fat32;
        boot_record_fat16_t fat16;
    } fat;

} __attribute__ ((packed)) boot_record_t;

/*
 * Partition block.
 */
typedef struct {
    uint8_t     bootable;
    uint8_t     head_start;
    uint8_t     sector_start;
    uint8_t     cyl_start;
    uint8_t     os_id;
    uint8_t     head_end;
    uint8_t     sector_end;
    uint8_t     cyl_end;
    /*
     * Relative sector (to start of partition -- also equals the partition's
     * starting LBA value)
     */
    uint32_t    LBA;
    uint32_t    sectors_in_partition;
} __attribute__ ((packed)) part_t;

/*
 * FAT structures
 */
typedef struct fat_file_date {
    uint32_t    day:5;
    uint32_t    month:4;
    uint32_t    year:7;
} __attribute__ ((packed)) fat_file_date_t;

typedef struct fat_file_time {
    uint32_t    sec:5;
    uint32_t    min:6;
    uint32_t    hour:5;
} __attribute__ ((packed)) fat_file_time_t;

/*
 * File info
 */
typedef struct fat_file {
    /*
     * Not safe to make char as the deleted char is 0xE5.
     */
    uint8_t     name[8];
    uint8_t     ext[3];
    uint8_t     attr;
    uint8_t     winnt_flags;
    uint8_t     create_time_secs;
    uint16_t    create_time;
    uint16_t    create_data;
    uint16_t    last_access;
    uint16_t    h_first_cluster;
    fat_file_time_t lm_time;
    fat_file_date_t lm_date;
    uint16_t    l_first_cluster;
    uint32_t    size;
} __attribute__ ((packed)) fat_dirent_t;

/*
 * VFAT, long file info
 */
typedef struct fat_file_long {
    uint8_t     order;
    uint16_t    first_5[5];
    uint8_t     attr;
    uint8_t     long_entry_type;
    uint8_t     checksum;
    uint16_t    next_6[6];
    uint16_t    zeros;
    uint16_t    final_2[2];
} __attribute__ ((packed)) fat_dirent_long_t;

/*
 * Comes before the FAT.
 */
typedef struct {
    uint8_t  signature1[4]; /* 0x41615252L */
    uint32_t reserved1[120];/* Nothing as far as I can tell */
    uint8_t  signature2[4]; /* 0x61417272L */
    uint32_t free_clusters; /* Free cluster count.  -1 if unknown */
    uint32_t next_cluster;  /* Most recently allocated cluster */
    uint32_t reserved2[4];
} fat_fsinfo;

/*
 * For walking dirs and maintaining context whilst doing so.
 */
typedef struct disk_walk_args_t_ {
    boolean print;
    boolean hexdump;
    boolean cat;
    boolean extract;
    boolean remove;
    boolean add;
    boolean is_intermediate_dir;
    boolean find;
    boolean stop_walk;
    boolean walk_whole_tree;
    fat_dirent_t dirent;
    char *add_dir;
    char *source;
} disk_walk_args_t;

/*
 * Used to represent a dirent chain, all tied together into one contiguous 
 * block of memory to make modifications simple when considering dirents 
 * spread over cluster boundaries.
 */
typedef struct {
    /*
     * Contigupous block of dirents and a copy of detect changes.
     */
    fat_dirent_t *dirents;

    /*
     * Where this directory begins.
     */
    uint32_t cluster;

    /*
     * Most dir cluster chains we can support.
     */
    uint32_t sector[MAX_DIRENT_BLOCK];
    uint32_t sectors[MAX_DIRENT_BLOCK];
    uint32_t number_of_chains;
    uint32_t number_of_dirents;

    /*
     * Need writing to disk.
     */
    boolean modified;
} dirent_t;

/*
 * A single sector being cached with its data.
 */
typedef struct tree_sector_cache_node_ {
    tree_key_int tree;
    uint8_t *buf;
} tree_sector_cache_node;

/*
 * My disk structure context.
 */
typedef struct disk_t_ {
    /*
     * Disk image.
     */
    const char *filename;

    /*
     * Offset from disk to FAT in bytes
     */
    int64_t offset;

    /*
     * Boot block
     */
    boot_record_t *mbr;

    /*
     * First 512 bytes
     */
    uint8_t *sector0;

    /*
     * Which partition is this disk.
     */
    uint32_t partition;
    boolean partition_set;

    /*
     * Partitions
     */
    part_t *parts[MAX_PARTITON];

    /*
     * FAT as read from disk.
     */
    uint8_t *fat;

    /*
     * Flags
     */
    boolean do_not_output_add_and_remove_while_replacing;

    /*
     * To speed up disk reads of sectors.
     */
    tree_root *tree_sector_cache;
} disk_t;

/*
 * FS types.
 */
enum {
    DISK_Empty		        = 0x00,
    DISK_FAT12		        = 0x01,
    DISK_XENIX_root		= 0x02,
    DISK_XENIX_usr		= 0x03,
    DISK_Small_FAT16	        = 0x04,
    DISK_Extended		= 0x05,
    DISK_FAT16		        = 0x06,
    DISK_HPFS_NTFS		= 0x07,
    DISK_AIX		        = 0x08,
    DISK_AIX_bootable	        = 0x09,
    DISK_OS_2_boot_mgr	        = 0x0a,
    DISK_FAT32		        = 0x0b,
    DISK_FAT32_LBA		= 0x0c,
    DISK_FAT16_LBA		= 0x0e,
    DISK_Extended_LBA	        = 0x0f,
    DISK_OPUS		        = 0x10,
    DISK_Hidden_FAT12	        = 0x11,
    DISK_Compaq_diag	        = 0x12,
    DISK_Hidd_Sm_FAT16	        = 0x14,
    DISK_Hidd_FAT16		= 0x16,
    DISK_Hidd_HPFS_NTFS	        = 0x17,
    DISK_AST_SmartSleep	        = 0x18,
    DISK_Hidd_FAT32		= 0x1b,
    DISK_Hidd_FAT32_LBA	        = 0x1c,
    DISK_Hidd_FAT16_LBA	        = 0x1e,
    DISK_NEC_DOS		= 0x24,
    DISK_Plan_9		        = 0x39,
    DISK_PMagic_recovery	= 0x3c,
    DISK_Venix_80286	        = 0x40,
    DISK_PPC_PReP_Boot	        = 0x41,
    DISK_SFS		        = 0x42,
    DISK_QNX4_x		        = 0x4d,
    DISK_QNX4_x_2nd_part	= 0x4e,
    DISK_QNX4_x_3rd_part	= 0x4f,
    DISK_OnTrack_DM		= 0x50,
    DISK_OnTrackDM6_Aux1	= 0x51,
    DISK_CP_M		        = 0x52,
    DISK_OnTrackDM6_Aux3	= 0x53,
    DISK_OnTrack_DM6	        = 0x54,
    DISK_EZ_Drive		= 0x55,
    DISK_Golden_Bow		= 0x56,
    DISK_Priam_Edisk	        = 0x5c,
    DISK_SpeedStor		= 0x61,
    DISK_GNU_HURD_SysV	        = 0x63,
    DISK_Netware_286	        = 0x64,
    DISK_Netware_386	        = 0x65,
    DISK_DiskSec_MltBoot	= 0x70,
    DISK_PC_IX		        = 0x75,
    DISK_Minix__1_4a	        = 0x80,
    DISK_Minix__1_4b	        = 0x81,
    DISK_Linux_swap		= 0x82,
    DISK_Linux		        = 0x83,
    DISK_OS_2_hidden_C 	        = 0x84,
    DISK_Linux_extended	        = 0x85,
    DISK_NTFS_volume_set1	= 0x86,
    DISK_NTFS_volume_set2	= 0x87,
    DISK_Linux_plaintext	= 0x88,
    DISK_Linux_LVM		= 0x8e,
    DISK_Amoeba		        = 0x93,
    DISK_Amoeba_BBT		= 0x94,
    DISK_BSD_OS		        = 0x9f,
    DISK_Thinkpad_hib	        = 0xa0,
    DISK_FreeBSD		= 0xa5,
    DISK_OpenBSD		= 0xa6,
    DISK_NeXTSTEP		= 0xa7,
    DISK_Darwin_UFS		= 0xa8,
    DISK_NetBSD		        = 0xa9,
    DISK_Darwin_boot	        = 0xab,
    DISK_BSDI_fs		= 0xb7,
    DISK_BSDI_swap		= 0xb8,
    DISK_Boot_Wizard_Hid	= 0xbb,
    DISK_Solaris_boot	        = 0xbe,
    DISK_Solaris		= 0xbf,
    DISK_DRDOS_2_FAT12	        = 0xc1,
    DISK_DRDOS_2_smFAT16	= 0xc4,
    DISK_DRDOS_2_FAT16	        = 0xc6,
    DISK_Syrinx		        = 0xc7,
    DISK_Non_FS_data	        = 0xda,
    DISK_CP_M___CTOS	        = 0xdb,
    DISK_Dell_Utility	        = 0xde,
    DISK_BootIt		        = 0xdf,
    DISK_DOS_access		= 0xe1,
    DISK_DOS_R_O		= 0xe3,
    DISK_SpeedStor2		= 0xe4,
    DISK_BeOS_fs		= 0xeb,
    DISK_EFI_GPT		= 0xee,
    DISK_EFI_FAT		= 0xef,
    DISK_Lnx_PA_RISC_bt	        = 0xf0,
    DISK_SpeedStor3		= 0xf1,
    DISK_DOS_secondary	        = 0xf2,
    DISK_SpeedStor4		= 0xf4,
    DISK_Lnx_RAID_auto	        = 0xfd,
    DISK_LANstep		= 0xfe,
    DISK_XENIX_BBT		= 0xff,
};

const char *msdos_get_systype(uint32_t index);
uint8_t msdos_parse_systype(char *in);
const char *msdos_get_media_type(uint32_t index);
uint32_t cluster_to_sector(disk_t *disk, uint32_t cluster);
uint8_t *disk_read_from(disk_t *disk, uint64_t offset, uint64_t len);
boolean sector_cache_add(disk_t *disk, uint32_t sector, uint8_t *buf);
void sectors_cache_add(disk_t * disk, uint32_t sector, uint32_t count,
                       uint8_t *buf);
uint8_t *sector_cache_find(disk_t *disk, uint32_t sector);
void sector_cache_destroy(disk_t *disk);
uint8_t *sector_read(disk_t *disk, uint32_t sector_, uint32_t count);
uint8_t *cluster_read(disk_t *disk, uint32_t cluster, uint32_t count);
boolean disk_write_at(disk_t *disk, uint64_t offset,
                      uint8_t *data, uint64_t len);
boolean sector_write(disk_t *disk, uint32_t sector_, uint8_t *data,
                     uint32_t count);
boolean sector_pre_write_print_dirty_sectors(disk_t *disk, uint32_t sector_,
                     uint8_t *data, uint32_t count);
boolean sector_write_no_cache(disk_t *disk, uint32_t sector, uint8_t *data,
                              uint32_t count);
boolean cluster_write(disk_t *disk, uint32_t cluster, uint8_t *data,
                      uint32_t count);
boolean cluster_write_no_cache(disk_t *disk, uint32_t cluster, uint8_t *data,
                               uint32_t count);
boolean disk_hex_dump(disk_t *disk, void *addr, uint64_t offset, uint64_t len);
boolean disk_cat(disk_t *disk, void *addr, uint64_t offset, uint64_t len);
uint32_t sector_size(disk_t *disk);
uint32_t cluster_size(disk_t *disk);
uint64_t disk_size(disk_t *disk);
uint32_t sector_reserved_count(disk_t *disk);
uint32_t sector_root_dir(disk_t *disk);
uint32_t sector_offset(disk_t *disk);
uint32_t sector_first_data_sector(disk_t *disk);
uint64_t sector_count_data(disk_t *disk);
uint64_t sector_count_total(disk_t *disk);
uint32_t root_dir_size_bytes(disk_t *disk);
uint32_t root_dir_size_sectors(disk_t *disk);
uint32_t total_clusters(disk_t *disk);
boolean partition_table_read(disk_t *disk);
boolean partition_table_write(disk_t *disk);
boolean partition_table_print(disk_t *disk);
boolean dos_dir_is_subset_of_dir(const char *a, const char *b);
