/*
 * Copyright (C) 2013 Neil McGill
 *
 * See the LICENSE file for license.
 */

#include "config.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <fcntl.h>
#include "main.h"
#include <unistd.h>
#include <ctype.h>

#include "disk.h"
#include "fat.h"

/*
 * FAT constants
 */
static const uint32_t FAT_DIRENT_SIZE               = 32;
static const uint32_t FAT_DOS_MAX_FILENAME_LEN      = 11;
static const uint32_t FAT_VFAT_FILENAME_FRAG_LEN    = 13;
static const uint32_t FAT_FILE_DELETE_CHAR          = 0xE5;
#if 0
static const uint32_t FAT_FILE_VFAT_CHAR            = 0x01;
#endif

/*
 * FAT file attr flags.
 */
#if 0
static const uint32_t FAT_ATTR_IS_READ_ONLY         = 0x01;
static const uint32_t FAT_ATTR_IS_HIDDEN            = 0x02;
static const uint32_t FAT_ATTR_IS_SYSTEM            = 0x04;
static const uint32_t FAT_ATTR_IS_LABEL             = 0x08;
#endif
static const uint32_t FAT_ATTR_IS_DIR               = 0x10;
static const uint32_t FAT_ATTR_IS_ARCHIVE           = 0x20;

/*
 * Prototypes.
 */
static boolean dirent_has_extension(fat_dirent_t *dirent);
static uint32_t vfat_fragments(const char *vfat_filename);
static char *dos_last_dot(char *in);
static char *dirent_read_name(disk_t *disk, fat_dirent_t *dirent,
                              char *vfat_filename);
static boolean dos_file_match(const char *a, const char *b, boolean is_dir);

/*
 * fat_type
 *
 * Work out the type of FAT on this disk based on number of clusters.
 */
uint32_t fat_type (disk_t *disk)
{
    uint32_t fat_type;

    if (disk->partition_set && disk->parts) {
        switch (disk->parts[disk->partition]->os_id) {
            case DISK_FAT12:
                return (12);

            case DISK_FAT16:
            case DISK_FAT16_LBA:
                return (16);

            case DISK_FAT32:
            case DISK_FAT32_LBA:

                /*
                 * Just in case this is a misconfigured disk with FAT32 in the 
                 * partition table, yet FAT16 is inferred from the FS, then 
                 * use FAT12/16 as inferred to avoid corrupting ths disk.
                 */
                if (disk->mbr->fat_size_sectors) {
                    /*
                     * We can only get here for FAT12/16. FAT32 will have the
                     * fat size sectors as 0.
                     */
                    if (total_clusters(disk) < 4085) {
                        fat_type = 12;
                    } else {
                        fat_type = 16;
                    }

                    return (fat_type);
                }

                return (32);
        }
    }

    if (disk->mbr->fat_size_sectors) {
        if (total_clusters(disk) < 4085) {
            fat_type = 12;
        } else {
            fat_type = 16;
        }

        return (fat_type);
    }

    if (total_clusters(disk) < 4085) {
        fat_type = 12;
    } else if (total_clusters(disk) < 65525) {
        fat_type = 16;
    } else {
        fat_type = 32;
    }

    return (fat_type);
}

/*
 * sector_root_dir
 *
 * The first data sector (that is, the first sector in which directories and
 * files may be stored):
 */
uint32_t sector_root_dir (disk_t *disk)
{
    return (disk->mbr->reserved_sector_count +
            (disk->mbr->number_of_fats * fat_size_sectors(disk)));
}

/*
 * sector_first_data_sector
 *
 * The first data sector (that is, the first sector in which directories and
 * files may be stored):
 */
uint32_t sector_first_data_sector (disk_t *disk)
{
    uint32_t total;

    total = disk->mbr->reserved_sector_count +
            (disk->mbr->number_of_fats * fat_size_sectors(disk));

    /*
     * No root dir for FAT32.
     */
    if (fat_type(disk) == 32) {
        return (total);
    }

    return (total + root_dir_size_sectors(disk));
}

/*
 * sector_count_data
 *
 * The total number of data sectors:
 */
uint64_t sector_count_data (disk_t *disk)
{
    return (sector_count_total(disk) -
            ((uint64_t)disk->mbr->reserved_sector_count +
            ((uint64_t)disk->mbr->number_of_fats * fat_size_sectors(disk)) +
            (uint64_t) root_dir_size_sectors(disk)));
}

/*
 * fat_size_bytes
 *
 * How large is the FAT table in bytes?
 */
uint64_t fat_size_bytes (disk_t *disk)
{
    return (fat_size_sectors(disk) * disk->mbr->sector_size);
}

/*
 * fat_size_sectors
 *
 * The number of blocks occupied by one copy of the FAT.
 */
uint64_t fat_size_sectors (disk_t *disk)
{
    if (disk->mbr->fat_size_sectors) {
        return (disk->mbr->fat_size_sectors);
    } else {
        return (disk->mbr->fat.fat32.fat_size_sectors);
    }
}

/*
 * root_dir_size_bytes
 *
 * The size of the root directory (unless you have FAT32, in which case the
 * size will be 0):
 */
uint32_t root_dir_size_bytes (disk_t *disk)
{
    if (!sector_size(disk)) {
        ERR("Boot record, sector size is 0 when calculating root dir size");
        return (0);
    }

    return (disk->mbr->number_of_dirents * FAT_DIRENT_SIZE);
}

/*
 * cluster_next
 *
 * Given a cluster, return the next cluster.
 */
static uint32_t cluster_next (disk_t *disk, uint32_t cluster)
{
    uint32_t fat_byte_offset;
    uint32_t cluster_next;
    uint8_t *fat;

    /*
     * Find the array index of the current cluster.
     */
    if (fat_type(disk) == 12) {
        fat_byte_offset = cluster + (cluster / 2); // multiply by 1.5
    } else if (fat_type(disk) == 16) {
        fat_byte_offset = cluster * sizeof(uint16_t);
    } else if (fat_type(disk) == 32) {
        fat_byte_offset = cluster * sizeof(uint32_t);
    } else {
        DIE("bug");
    }

    /*
     * Only look at the first FAT. I think this is ok. The others are backups.
     */
    fat = disk->fat;

    fat_byte_offset = fat_byte_offset % fat_size_bytes(disk);

    /*
     * Find the cluster in this next array index.
     */
    if (fat_type(disk) == 12) {
        cluster_next = *(uint16_t*) (fat + fat_byte_offset);

        if (cluster & 0x0001) {
            cluster_next = cluster_next >> 4;
        } else {
            cluster_next = cluster_next & 0x0FFF;
        }
    } else if (fat_type(disk) == 16) {
        cluster_next = *(uint16_t*) (fat + fat_byte_offset);
    } else if (fat_type(disk) == 32) {
        cluster_next = (*((uint32_t*) (fat + fat_byte_offset))) & 0x0FFFFFFF;
    } else {
        DIE("bug");
    }

    return (cluster_next);
}

/*
 * cluster_next_set
 *
 * Given a cluster, set what it points to.
 */
static uint32_t cluster_next_set (disk_t *disk,
                                  uint32_t cluster,
                                  uint32_t cluster_next,
                                  boolean update_fat)
{
    uint32_t fat_byte_offset;
    uint8_t *fat;
    uint16_t old;

    /*
     * Find the array index of the current cluster.
     */
    if (fat_type(disk) == 12) {
        fat_byte_offset = cluster + (cluster / 2); // multiply by 1.5
    } else if (fat_type(disk) == 16) {
        fat_byte_offset = cluster * sizeof(uint16_t);
    } else if (fat_type(disk) == 32) {
        fat_byte_offset = cluster * sizeof(uint32_t);
    } else {
        DIE("bug");
    }

    /*
     * Only look at the first FAT. I think this is ok. The others are backups.
     */
    fat = disk->fat;

    if (fat_byte_offset > fat_size_bytes(disk)) {
        /*
         * Propbably not critical, but not ideal either. Means we've not
         * sized the FAT enough.
         */
        DIE("Trying to access cluster %" PRIu32 " which "
            "is bigger than the FAT. "
            "Total clusters on disk %" PRIu32 ". "
            "FAT size in clusters %" PRIu64 ". "
            "FAT size in bytes %" PRIu64 ". "
            "FAT offset being accessed %" PRIu32 ".",
            cluster,
            total_clusters(disk),
            fat_size_sectors(disk),
            fat_size_bytes(disk),
            fat_byte_offset);

        return (0);
    }

    fat_byte_offset = fat_byte_offset % fat_size_bytes(disk);

    /*
     * Find the cluster in this next array index.
     */
    if (fat_type(disk) == 12) {
        /*
         * Need to mask out the old cluster but keep the surrounding bits
         * for the cluster that is sharing these 16 bits. What a horrible
         * system.
         */
        memcpy(&old, fat + fat_byte_offset, 2);

        old = *(uint16_t*) (fat + fat_byte_offset);

        if (cluster & 0x0001) {
            cluster_next = cluster_next << 4;
            cluster_next |= old & 0x000F;
        } else {
            cluster_next = cluster_next & 0x0FFF;
            cluster_next |= old & 0xF000;
        }

        memcpy(fat + fat_byte_offset, &cluster_next, 2);
    } else if (fat_type(disk) == 16) {
        uint16_t next = cluster_next;

        memcpy(fat + fat_byte_offset, &next, 2);
    } else if (fat_type(disk) == 32) {
        memcpy(fat + fat_byte_offset, &cluster_next, 4);
    } else {
        DIE("bug");
    }

    if (!update_fat) {
        return (cluster_next);
    }

    /*
     * Update just the touched sectors on the FAT.
     */
    uint32_t sector_start = fat_byte_offset / sector_size(disk);
    uint32_t sector_end = (fat_byte_offset + 4) / sector_size(disk);
    uint32_t sector_max;

    uint8_t *data = ((uint8_t*) disk->fat) +
                    (sector_start * sector_size(disk));

    sector_start += sector_reserved_count(disk);
    sector_end += sector_reserved_count(disk);
    sector_max = sector_reserved_count(disk) + fat_size_sectors(disk);

    if (sector_end > sector_max) {
        DIE("Failed to update FAT sector %" PRIu32 " .. %" PRIu32 
            " max %" PRIu32 "",
            sector_start, sector_end, sector_max);
        sector_end = sector_max;
    }

    if (!sector_write(disk, sector_start,
                      data, sector_end - sector_start + 1)) {

        DIE("Failed to update FAT sector %" PRIu32 " .. %" PRIu32 
            " max %" PRIu32 "",
            sector_start, sector_end, sector_max);
    }

    return (cluster_next);
}

/*
 * cluster_alloc
 *
 * Find a free cluster.
 */
static uint32_t cluster_alloc (disk_t *disk)
{
    /*
     * Start from the first valid cluster.
     */
    static uint32_t last_cluster = 2;
    uint32_t fat_byte_offset;
    uint32_t cluster_next;
    uint32_t cluster;
    uint8_t *fat;

redo:
    /*
     * Try from the last cluster found to speed things up and give us
     * a chance for things to be sequential.
     */
    for (cluster = last_cluster; cluster < total_clusters(disk); cluster++) {
        /*
         * Find the array index of the current cluster.
         */
        if (fat_type(disk) == 12) {
            /*
             * Ignore root cluster if FAT32.
             */
            if (cluster == disk->mbr->fat.fat32.root_cluster) {
                continue;
            }

            fat_byte_offset = cluster + (cluster / 2); // multiply by 1.5
        } else if (fat_type(disk) == 16) {
            fat_byte_offset = cluster * sizeof(uint16_t);
        } else if (fat_type(disk) == 32) {
            fat_byte_offset = cluster * sizeof(uint32_t);
        } else {
            DIE("bug");
        }

        /*
         * Only look at the first FAT. I think this is ok. The others are
         * backups.
         */
        fat = disk->fat;

        fat_byte_offset = fat_byte_offset % fat_size_bytes(disk);

        /*
         * Find the cluster in this next array index.
         */
        if (fat_type(disk) == 12) {
            cluster_next = *(uint16_t*) (fat + fat_byte_offset);

            if (cluster & 0x0001) {
                cluster_next = cluster_next >> 4;
            } else {
                cluster_next = cluster_next & 0x0FFF;
            }

        } else if (fat_type(disk) == 16) {
            cluster_next = *(uint16_t*) (fat + fat_byte_offset);
        } else if (fat_type(disk) == 32) {
            cluster_next = (*((uint32_t*)
                              (fat + fat_byte_offset))) & 0x0FFFFFFF;
        } else {
            DIE("bug");
        }

        if (!cluster_next) {
#ifdef FAT_WRITE_EMPTY_CLUSTERS_ON_ALLOC
            /*
             * This is too slow for file importing. It is needed when making
             * writing unless paranoid about removing old info.
             */
            uint8_t *tmp;

            tmp = myzalloc(cluster_size(disk), __FUNCTION__);

            cluster_write(disk, cluster - 2, tmp, 1);

            myfree(tmp);
#endif
            DBG2("Allocated cluster %" PRIu32, cluster);

            /*
             * Next time, search from this cluster onwards, for speed.
             */
            last_cluster = cluster;

            return (cluster);
        }
    }

    /*
     * Out of clusters? Retry once.
     */
    if (last_cluster > 2) {
        last_cluster = 2;
        goto redo;
    }

    ERR("Out of clusters, total clusters on disk, %u, "
        "data sectors %" PRIu64 ", "
        "sectors per cluster %u",
        total_clusters(disk),
        sector_count_data(disk),
        disk->mbr->sectors_per_cluster);

    return (0);
}

/*
 * cluster_how_many_free
 *
 * How many free clusters are there on disk?
 */
uint64_t cluster_how_many_free (disk_t *disk)
{
    uint32_t fat_byte_offset;
    uint32_t cluster_next;
    uint32_t cluster;
    uint8_t *fat;
    uint64_t free = 0;

    /*
     * Try from the last cluster found to speed things up and give us
     * a chance for things to be sequential.
     */
    for (cluster = 2; cluster < total_clusters(disk); cluster++) {
        /*
         * Find the array index of the current cluster.
         */
        if (fat_type(disk) == 12) {
            /*
             * Ignore root cluster if FAT32.
             */
            if (cluster == disk->mbr->fat.fat32.root_cluster) {
                continue;
            }

            fat_byte_offset = cluster + (cluster / 2); // multiply by 1.5
        } else if (fat_type(disk) == 16) {
            fat_byte_offset = cluster * sizeof(uint16_t);
        } else if (fat_type(disk) == 32) {
            fat_byte_offset = cluster * sizeof(uint32_t);
        } else {
            DIE("bug");
        }

        /*
         * Only look at the first FAT. I think this is ok. The others are
         * backups.
         */
        fat = disk->fat;

        fat_byte_offset = fat_byte_offset % fat_size_bytes(disk);

        /*
         * Find the cluster in this next array index.
         */
        if (fat_type(disk) == 12) {
            cluster_next = *(uint16_t*) (fat + fat_byte_offset);

            if (cluster & 0x0001) {
                cluster_next = cluster_next >> 4;
            } else {
                cluster_next = cluster_next & 0x0FFF;
            }

        } else if (fat_type(disk) == 16) {
            cluster_next = *(uint16_t*) (fat + fat_byte_offset);
        } else if (fat_type(disk) == 32) {
            cluster_next = (*((uint32_t*)
                              (fat + fat_byte_offset))) & 0x0FFFFFFF;
        } else {
            DIE("bug");
        }

        if (!cluster_next) {
            free++;
        }
    }

    return (free);
}

/*
 * Read and cache the FAT
 */
void fat_read (disk_t *disk)
{
    if (disk->fat) {
        return;
    }

    if (disk->partition_set && disk->parts) {
        switch (disk->parts[disk->partition]->os_id) {
        case DISK_FAT12:
        case DISK_FAT16:
        case DISK_FAT16_LBA:
        case DISK_FAT32:
        case DISK_FAT32_LBA:
            break;

        default:
            ERR("Cannot read fat at partition %u sector %" PRIu32 "", 
                disk->partition,
                sector_reserved_count(disk));

            disk->fat = 0;
            return;
        }
    }

    DBG2("Read FAT, %" PRIu64 " sectors...",
         sector_reserved_count(disk) * fat_size_sectors(disk));

    disk->fat = sector_read(disk,
                            sector_reserved_count(disk),
                            fat_size_sectors(disk));
    if (!disk->fat) {
        ERR("Cannot read fat at sector %" PRIu32 "", 
            sector_reserved_count(disk));
        return;
    }
}

/*
 * fat_write
 *
 * Update the FAT on disk with any sectors that are dirtied.
 */
void fat_write (disk_t *disk)
{
    uint32_t sector;
    uint32_t sectors;
    uint8_t *data;

    data = (uint8_t*) disk->fat;
    if (!data) {
        return;
    }

    sector = sector_reserved_count(disk);

    DBG2("FAT write");

    sectors = fat_size_sectors(disk);

    sector_pre_write_print_dirty_sectors(disk, sector, data, sectors);

    if (!sector_write(disk, sector, data, sectors)) {
        DIE("cannot write FAT at sector %" PRIu32 "", sector);
    }
}

/*
 * cluster_max
 *
 * For the given FS, what is the max cluster?
 */
static uint32_t cluster_max (disk_t *disk)
{
    if (fat_type(disk) == 12) {
        return (0xFF8);
    } else if (fat_type(disk) == 16) {
        /*
         * FFF0-FFF6: reserved, FFF7: bad cluster, FFF8-FFFF
         */
        return (0xFFF8);
    } else if (fat_type(disk) == 32) {
        /*
         * Some operating systems use fff, ffff, xfffffff as end of 
         * clusterchain markers, but various common utilities may use 
         * different values.  
         * 
         * Linux has always used ff8, fff8, but it now appears that some MP3 
         * players fail to work unless fff etc. is used.
         *
         * However, grub chokes if 0x0FFFFFFF is used. Stick with 0x0FFFFFF8
         * for now until someone complains.
         */
        return (0x0FFFFFF8);
    } else {
        DIE("cluster max, channot determine disk type");
    }
}

/*
 * cluster_endchain
 *
 * Is this cluster the end of a chain?
 */
static uint32_t cluster_endchain (disk_t *disk, uint32_t cluster)
{
    if (fat_type(disk) == 32) {
        /*
         * Root cluster sector cannot be a next sector.
         */
        if (cluster <= 2) {
            return (true);
        }
    } else {
        if (cluster < 2) {
            return (true);
        }
    }

    if (fat_type(disk) == 12) {
        /*
         * FF0-FF6: reserved, FF7: bad cluster, FF8-FFF
         */
        if (cluster >= 0xFF0) {
            return (true);
        }
    } else if (fat_type(disk) == 16) {
        /*
         * FFF0-FFF6: reserved, FFF7: bad cluster, FFF8-FFFF
         */
        if (cluster >= 0xFFF0) {
            return (true);
        }
    } else if (fat_type(disk) == 32) {
        if (cluster >= 0x0FF8FFF8) {
            return (true);
        }
    } else {
        DIE("cluster end chain, channot determine disk type");
    }

    return (false);
}

/*
 * dirent_first_cluster
 *
 * Where does the file data begin?
 */
static uint32_t dirent_first_cluster (const fat_dirent_t *dirent)
{
    return ((dirent->h_first_cluster << 16) | dirent->l_first_cluster);
}

/*
 * dirent_attr_string
 *
 * Expand file attributes.
 */
static const char *dirent_attr_string (const fat_dirent_t *dirent)
{
    /*
     * 0    0x01    Read Only. (Since DOS 2.0) If this bit is set, the
     * operating system will not allow a file to be opened for modification.
     *
     * Deliberately setting this bit for files which will not be written to
     * (executables, shared libraries and data files) may help avoid problems
     * with concurrent file access in multi-tasking, multi-user or network
     * environments with applications not specifically designed to work in
     * such environments (i.e. non-SHARE-enabled programs).
     *
     * 1    0x02    Hidden. Hides files or directories from normal directory
     * views.
     *
     * Under DR DOS 3.31 and higher, under PalmDOS, Novell DOS, OpenDOS,
     * Concurrent DOS, Multiuser DOS, REAL/32, password protected files and
     * directories also have the hidden attribute set.[65] Password-aware
     * operating systems should not hide password-protected files from
     * directory views, even if this bit may be set. The password protection
     * mechanism does not depend on the hidden attribute being set up to
     * including DR-DOS 7.03, but if the hidden attribute is set, it should
     * not be cleared for any password-protected files.
     *
     * 2    0x04    System. Indicates that the file belongs to the system and
     * must not be physically moved (f.e. during defragmentation), because
     * there may be references into the file using absolute addressing
     * bypassing the file system (boot loaders, kernel images, swap files,
     * extended attributes, etc.).
     *
     * 3    0x08    Volume Label. (Since DOS 2.0) Indicates an optional
     * directory volume label, normally only residing in a volume's root
     * directory. Ideally, the volume label should be the first entry in the
     * directory (after reserved entries) in order to avoid problems with VFAT
     * LFNs. If this volume label is not present, some systems may fall back
     * to display the partition volume label instead, if a EBPB is present in
     * the boot sector (not present with some non-bootable block device
     * drivers, and possibly not writeable with boot sector write protection).
     * Even if this volume label is present, partitioning tools like FDISK may
     * display the partition volume label instead. The entry occupies a
     * directory entry but has no file associated with it. Volume labels have
     * a filesize entry of zero.
     *
     * Pending delete files and directories under DELWATCH have the volume
     * attribute set until they are purged or undeleted.[65]
     *
     * 4    0x10    Subdirectory. (Since DOS 2.0) Indicates that the
     * cluster-chain associated with this entry gets interpreted as
     * subdirectory instead of as a file. Subdirectories have a filesize entry
     * of zero.
     *
     * 5    0x20    Archive. (Since DOS 2.0) Typically set by the operating
     * system as soon as the file is created or modified to mark the file as
     * "dirty", and reset by backup software once the file has been backed up
     * to indicate "pure" state.
     *
     * 6    0x40    Device (internally set for character device names found in
     * filespecs, never found on disk), must not be changed by disk tools.
     *
     * 7    0x80    Reserved, must not be changed by disk tools.
     */
    static char attributes[] = { 'r', 'h', 's', 'v', 'd', 'a', 'D' };
    static char attrs[sizeof(attributes)];
    uint32_t attr;
    uint32_t i;

    attr = dirent->attr;

    for (i = 0; i < sizeof(attributes); i++) {
        if (attr & 1) {
            attrs[i] = attributes[i];
        } else {
            attrs[i] = '-';
            attr >>= 1;
        }
    }

    return (attrs);
}

/*
 * dirent_month
 *
 * Return the month for this file.
 */
static const char *dirent_month (uint32_t month)
{
    const char *str_month[12] = {
        "Jan", "Feb", "Mar", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep",
        "Oct", "Nov", "Dec"
    };

    const char *month_str = "???";

    if ((month > 0) && (month <= 12)) {
        month_str = str_month[month - 1];
    }

    return (month_str);
}

/*
 * dirent_name_trim
 *
 * Trim spaces from the FAT file name.
 */
static char *dirent_name_trim (char *string)
{
    size_t len;

    len = strlen(string);
    while (len-- > 0) {
        if (string[len] != ' ') {
            return (string);
        }

        string[len] = '\0';
    }

    return (string);
}

/*
 * dirent_name_copy
 *
 * Alloc a filename from the dir entry, FAT or VFAT.
 */
static char *dirent_name_copy (fat_dirent_t *dirent)
{
    char *name;
    uint32_t i;

    name = (typeof(name)) myzalloc(MAX_STR, "FAT filename");

    for (i = 0; i < sizeof(dirent->name); i++) {
        name[i] = dirent->name[i];
    }

    name[i] = '\0';

    if (dirent_has_extension(dirent)) {
        dirent_name_trim(name);

        strcat(name, ".");

        for (i = 0; i < sizeof(dirent->ext); i++) {
            char tmp[2];

            tmp[0] = dirent->ext[i];
            tmp[1] = 0;

            strcat(name, tmp);
        }
    }

    /*
     * Here we assume dirent_has_extension will return true only if
     * there's a valid (non-blank, non-nil) ext, tx Andy Dalton.
     */
    if (dirent_has_extension(dirent)) {
        dirent_name_trim(name);

        size_t index = strlen(name);

        name[index++] = '.';

        for (i = 0; i < sizeof(dirent->ext); ++i) {
            name[index + i] = dirent->ext[i];
        }

        name[index + i] = '\0';
    }

    return (dirent_name_trim(name));
}

/*
 * dirent_is_dir
 *
 * Dirent is a dir?
 */
static boolean dirent_is_dir (fat_dirent_t *dirent)
{
    if (dirent->attr & FAT_ATTR_IS_DIR) {
        return (true);
    }

    return (false);
}

/*
 * dirent_has_extension
 *
 * Does the file have a non null three letter extension?
 */
static boolean dirent_has_extension (fat_dirent_t *dirent)
{
    if ((dirent->ext[0] == ' ') &&
        (dirent->ext[1] == ' ') &&
        (dirent->ext[2] == ' ')) {
        return (false);
    } else {
        return (true);
    }
}

/*
 * file_import
 *
 * Read in the file and create it (or directory). For directories we make
 * the subdir too.
 */
static uint32_t file_import (disk_t *disk,
                             disk_walk_args_t *args,
                             fat_dirent_t *dirent,
                             const char *filename,
                             uint32_t parent_cluster,
                             uint32_t depth)
{
    char *base = mybasename(filename, __FUNCTION__);
    fat_dirent_t *dirent_in = dirent;
    fat_dirent_long_t *fat_dirent;
    char tmp[MAX_STR] = {0};
    uint32_t cluster;
    uint8_t *data;
    uint32_t count;
    int32_t year;
    int32_t day;
    int32_t month;

    count = 0;

    strncpy(tmp, base, sizeof(tmp));

    /*
     * How many VFAT fragments?
     */
    const uint32_t fragments = vfat_fragments(filename);

    /*
     * Make the long VFAT filename out of fragments.
     */
    boolean first_fragment = true;

    int32_t fragment = fragments;

    while (fragment--) {
        /*
         * Null out the fragment.
         */
        fat_dirent = (typeof(fat_dirent)) dirent;

        /*
         * Sanity check.
         */
        if (fat_dirent->order &&
            (fat_dirent->order != FAT_FILE_DELETE_CHAR)) {
            hex_dump(fat_dirent, 0, sizeof(*dirent));

            DIE("overwriting an existing file, order bytes is set "
                "while adding %s", filename);
        }

        memset(fat_dirent, 0, sizeof(*fat_dirent));

        uint32_t index = (fragment * FAT_VFAT_FILENAME_FRAG_LEN);
        char c;

        /*
         * This logic is quite horrible in how we pad out names. Blame 
         * Microsoft.
         */
#define FAT_CHECK_END_OF_NAME(c)                            \
        if (index == strlen(tmp)) {                         \
            c = 0x00;                                       \
        } else if (index > strlen(tmp)) {                   \
            c = 0xFF;                                       \
        } else {                                            \
            c = tmp[index];                                 \
        }                                                   \
                                                            \
        index++;                                            \

        FAT_CHECK_END_OF_NAME(c);
        fat_dirent->first_5[0] = c;

        FAT_CHECK_END_OF_NAME(c);
        fat_dirent->first_5[1] = c;

        FAT_CHECK_END_OF_NAME(c);
        fat_dirent->first_5[2] = c;

        FAT_CHECK_END_OF_NAME(c);
        fat_dirent->first_5[3] = c;

        FAT_CHECK_END_OF_NAME(c);
        fat_dirent->first_5[4] = c;

        FAT_CHECK_END_OF_NAME(c);
        fat_dirent->next_6[0] = c;

        FAT_CHECK_END_OF_NAME(c);
        fat_dirent->next_6[1] = c;

        FAT_CHECK_END_OF_NAME(c);
        fat_dirent->next_6[2] = c;

        FAT_CHECK_END_OF_NAME(c);
        fat_dirent->next_6[3] = c;

        FAT_CHECK_END_OF_NAME(c);
        fat_dirent->next_6[4] = c;

        FAT_CHECK_END_OF_NAME(c);
        fat_dirent->next_6[5] = c;

        FAT_CHECK_END_OF_NAME(c);
        fat_dirent->final_2[0] = c;

        FAT_CHECK_END_OF_NAME(c);
        fat_dirent->final_2[1] = c;

        /*
         * Mark as a VFAT entry
         */
        fat_dirent->attr = 0x0F;
        fat_dirent->order = fragment + 1;

        if (first_fragment) {
            first_fragment = false;
            fat_dirent->order |= 0x40;
        }

        dirent++;
    }

    /*
     * Sanity check.
     */
    if (dirent->name[0] &&
        (dirent->name[0] != FAT_FILE_DELETE_CHAR)) {
        hex_dump(dirent, 0, sizeof(*dirent));

        DIE("overwriting an existing file, order bytes is set "
            "while adding dirent %s", filename);
    }

    /*
     * Now create the regular FAT name dirent.
     */
    memset(dirent, 0, sizeof(*dirent));

    strncpy((char*)dirent->name, tmp, sizeof(dirent->name));

    /*
     * DOS pads files with spaces.
     */
    memset(dirent->name, ' ', sizeof(dirent->name));
    memset(dirent->ext, ' ', sizeof(dirent->ext));

    /*
     * Add the name and extension and copy it over.
     */
    char *extension = 0;
    size_t c;

    /*
     * If not "." and not ".." add on the extension
     */
    if ((strcmp(filename, ".") != 0) && (strcmp(filename, "..") != 0)) {
        extension = dos_last_dot(base);

        if (extension) {
            strncpy((char*) dirent->ext,
                    extension + 1,
                    min(sizeof(dirent->ext), strlen(extension + 1)));

            for (c = 0; c < sizeof(dirent->ext); c++) {
                dirent->ext[c] = toupper(dirent->ext[c]);
            }
        }
    }

    /*
     * Copy the name over, making it upper case as we go.
     */
    boolean truncated = false;

    for (c = 0; c < strlen(base); c++) {
        if (extension) {
            if (base + c >= extension) {
                if (base[c] != '.') {
                    truncated = true;
                }

                break;
            }
        }

        if (c >= sizeof(dirent->name)) {
            truncated = true;
            break;
        }

        dirent->name[c] = toupper(base[c]);
    }

    /*
     * Fill ot the extension with pad chars.
     */
    for (c = 0; c < sizeof(dirent->ext); c++) {
        if (!dirent->ext[c]) {
            dirent->ext[c] = ' ';
        }
    }

    for (c = 0; c < sizeof(dirent->name); c++) {
        if (!dirent->name[c]) {
            dirent->name[c] = ' ';
        }
    }

    /*
     * If the name is truncated, DOS adds this dumb ~1 stuff.
     */
    if (truncated) {
        dirent->name[6] = '~';
        dirent->name[7] = '1';
    }

    /*
     * Add now do the checksum.
     */
    uint8_t sum = 0;

    for (c = 0; c < FAT_DOS_MAX_FILENAME_LEN; c++) {
        sum = (sum >> 1) + ((sum & 1) << 7);  /* rotate */
        sum += dirent->name[c];               /* add next name byte */
    }

    /*
     * And copy the checksum to each fragment. I think this is correct.
     */
    fragment = fragments;

    while (fragment--) {
        fat_dirent = (typeof(fat_dirent)) (dirent_in + fragment);

        fat_dirent->checksum = sum;
    }

    /*
     * Add modify time values.
     */
    if (file_mtime(args->source, &day, &month, &year)) {
        dirent->lm_date.year = year - 1980;
        dirent->lm_date.month = month;
        dirent->lm_date.day = day;
    }

    /*
     * Add dir or file.
     */
    if (args->is_intermediate_dir || dir_exists(args->source)) {
        dirent->attr = FAT_ATTR_IS_DIR;
    } else {
        dirent->size = (uint32_t) file_size(args->source);
        dirent->attr = FAT_ATTR_IS_ARCHIVE;
    }

    /*
     * Are we adding a dir?
     */
    if (args->is_intermediate_dir || dir_exists(args->source)) {
        /*
         * Yes.
         */
        if (!strcmp(filename, ".")) {
            /*
             * We're making a subdir.
             */
            dirent->h_first_cluster = (parent_cluster & 0xffff0000) >> 16;
            dirent->l_first_cluster = (parent_cluster & 0x0000ffff);

            DBG("mkdir %s" PRIu32 "", filename);

        } else if (!strcmp(filename, "..")) {
            /*
             * We're making a subdir.
             */
            dirent->h_first_cluster = (parent_cluster & 0xffff0000) >> 16;
            dirent->l_first_cluster = (parent_cluster & 0x0000ffff);

            DBG("mkdir %s" PRIu32 "", filename);
        } else {
            /*
             * Add a new dir with subdirs, . and ..
             */
            DBG("mkdir %-20s -> %-20s, "
                "dos name [%c%c%c%c%c%c%c%c.%c%c%c] fragments %" PRIu32 "",
                args->source, filename,
                dirent->name[0],
                dirent->name[1],
                dirent->name[2],
                dirent->name[3],
                dirent->name[4],
                dirent->name[5],
                dirent->name[6],
                dirent->name[7],
                dirent->ext[0],
                dirent->ext[1],
                dirent->ext[2],
                fragments);

            cluster = cluster_alloc(disk);
            if (!cluster) {
                DIE("Out of clusters/disk space when adding dir %s", filename);
            }

            dirent->h_first_cluster = (cluster & 0xffff0000) >> 16;
            dirent->l_first_cluster = (cluster & 0x0000ffff);

            cluster_next_set(disk, cluster, cluster_max(disk),
                             false /* update FAT */);

            /*
             * Add a cluster for the dirents for the . and .. subdirs.
             */
            data = (typeof(data)) myzalloc(cluster_size(disk), __FUNCTION__);

            /*
             * Add . and .. dirs
             */
            dirent = (fat_dirent_t *)
                            (((uint8_t*) data) + (0 * FAT_DIRENT_SIZE));
            file_import(disk, args, dirent, ".", cluster, depth + 1);
            dirent = (fat_dirent_t *)
                            (((uint8_t*) data) + (1 * FAT_DIRENT_SIZE));
            file_import(disk, args, dirent, "..", parent_cluster, depth + 1);

            if (!cluster_write(disk, cluster - 2, data, 1)) {
                DIE("cannot write dirent at cluster %" PRIu32 "", cluster);
            }

            myfree(data);
        }

        /*
         * Done!
         */
    } else {
        /*
         * Ok, adding a file.
         */
        DBG("mkfile %-20s -> %-20s, "
            "dos name [%c%c%c%c%c%c%c%c.%c%c%c] fragments %" PRIu32 "",
            args->source, filename,
            dirent->name[0],
            dirent->name[1],
            dirent->name[2],
            dirent->name[3],
            dirent->name[4],
            dirent->name[5],
            dirent->name[6],
            dirent->name[7],
            dirent->ext[0],
            dirent->ext[1],
            dirent->ext[2],
            fragments);

        boolean first = true;
        uint32_t last_cluster;
        int64_t len;

        /*
         * Read the whole file in one go.
         */
        data = file_read(args->source, &len);
        if (!data) {
            WARN("Failed to read local %s for placing on disk image", filename);
            return (0);
        }

        last_cluster = 0;
        cluster = 0;

        /*
         * How many clusters will the file need?
         */
        uint32_t cluster_count = (len + (cluster_size(disk) - 1)) /
                        cluster_size(disk);

        if (!cluster_count) {
            cluster_count = 1;
        }

        /*
         * An array of clusters we allocate and how much data is in each.
         * Only the last will be less than the cluster size.
         */
        uint32_t *cluster_array =
                    (typeof(cluster_array))
                    myzalloc(sizeof(uint32_t) * cluster_count, __FUNCTION__);

        uint32_t *cluster_len =
                    (typeof(cluster_len))
                    myzalloc(sizeof(uint32_t) * cluster_count, __FUNCTION__);

        uint32_t c = 0;

        /*
         * Allocate all clusters in one go so we can find out the contiguous 
         * clusters. Don't write to the disk yet.
         */
        while (len >= 0) {
            cluster_array[c] = cluster = cluster_alloc(disk);
            if (!cluster) {
                DIE("Out of clusters/disk space when adding file %s", filename);
            }

            if (last_cluster) {
                cluster_next_set(disk, last_cluster, cluster,
                                 false /* update FAT */);
            }

            /*
             * This line may seem not needed but is to prevent cluster alloc
             * returning this cluster again.
             */
            cluster_next_set(disk, cluster, cluster_max(disk),
                             false /* update FAT */);

            /*
             * Point the file at its first cluster.
             */
            if (first) {
                first = false;
                dirent->h_first_cluster = (cluster & 0xffff0000) >> 16;
                dirent->l_first_cluster = (cluster & 0x0000ffff);
            } else {
                if (!last_cluster) {
                    DIE("bug, no last cluster");
                }
            }

            /*
             * Save the size of the data in this cluster. Normally this is a 
             * full cluster except for the last fragment. Could do this 
             * better. Lazy, but simple.
             */
            if (len < cluster_size(disk)) {
                cluster_len[c] = len;
            } else {
                cluster_len[c] = cluster_size(disk);
            }

            len -= cluster_len[c++];

            /*
             * Record the cluster chain.
             */
            last_cluster = cluster;

            if (!len) {
                break;
            }
        }

        /*
         * Write contiguous cluster blocks of file data now. This should be 
         * quite fast.
         */
        uint32_t frag_size = cluster_size(disk);
        uint32_t block_size = 0;
        uint32_t data_size = 0;
        uint32_t frag_count = 0;
        int32_t cluster_start = -1;
        int32_t cluster_end = -1;

        for (c = 0; c < cluster_count; c++) {

            cluster = cluster_array[c];
            if (!cluster) {
                DIE("no cluster allocated when adding file %s", filename);
            }

            if (c < cluster_count - 1) {
                /*
                 * If the next cluster is contigious, tack it onto the block.
                 */
                if (cluster + 1 == cluster_array[c + 1]) {
                    if (cluster_start == -1) {
                        cluster_start = c;
                    }

                    cluster_end = c;
                    frag_count++;
                    continue;
                }
            }

            /*
             * Write x contiguous clusters of data.
             */
            if (!frag_count) {
                continue;
            }

            block_size = 0;
            data_size = 0;

            /*
             * Clean out those clusters so they will not be looked at later.
             */
            int32_t i;

            cluster = cluster_array[cluster_start];

            for (i = cluster_start; i <= cluster_end; i++) {
                block_size += frag_size;
                data_size += cluster_len[i];
                cluster_array[i] = 0;
                cluster_len[i] = 0;
            }

            /*
             * Write the block of data.
             */
            uint8_t *cluster_data =
                (typeof(cluster_data)) myzalloc(block_size, __FUNCTION__);

            memset(cluster_data, 0, block_size);
            memcpy(cluster_data, data + cluster_start * frag_size, data_size);

            cluster_write_no_cache(disk, cluster - 2, cluster_data, frag_count);

            myfree(cluster_data);

            frag_count = 0;
            cluster_start = -1;
            cluster_end = -1;
        }

        /*
         * Write remaining non contigious clusters. Slow.
         */
        for (c = 0; c < cluster_count; c++) {
            cluster = cluster_array[c];
            if (!cluster) {
                continue;
            }

            uint32_t data_size = cluster_len[c];
            uint32_t frag_size = cluster_size(disk);

            uint8_t *cluster_data = 
                (typeof(cluster_data)) myzalloc(frag_size, __FUNCTION__);

            memcpy(cluster_data, data + c * frag_size, data_size);

            cluster_write_no_cache(disk, cluster - 2, cluster_data, 1);

            myfree(cluster_data);
        }

        myfree(cluster_array);
        myfree(cluster_len);
        myfree(data);

        count++;
    }

    myfree(base);

    /*
     * What dirents did we make?
     */
    if (opt_debug3) {
        fragment = fragments;

        while (fragment--) {
            fat_dirent = (typeof(fat_dirent))
                            (dirent_in + fragments - fragment);

            hex_dump(fat_dirent,
		     (uint64_t) (uintptr_t) fat_dirent, sizeof(*fat_dirent));
        }

        hex_dump(dirent, (uint64_t) (uintptr_t) dirent, sizeof(*dirent));
    }

    return (count);
}

/*
 * dirent_entry_exists
 *
 * See if the given entry exists in the dir.
 */
static boolean dirent_entry_exists (disk_t *disk,
                                    dirent_t *dirents,
                                    const char *dir_name,
                                    const char *find)
{
    char vfat_filename[MAX_STR];
    char *dos_full_path_name;
    char *vfat_full_path_name;
    char *vfat_or_dos_name;
    char *dir_lower_name;
    fat_dirent_t *dirent;
    boolean found;
    uint32_t d;

    vfat_filename[0] = '\0';

    dir_lower_name = duplstr(dir_name, __FUNCTION__);
    found = false;

    for (d = 0; d < dirents->number_of_dirents; d++) {

        dirent = (fat_dirent_t *)
                (((uint8_t*) dirents->dirents) + (d * FAT_DIRENT_SIZE));

        vfat_or_dos_name = dirent_read_name(disk, dirent, vfat_filename);
        if (!vfat_or_dos_name) {
            continue;
        }

        if (dirent_is_dir(dirent)) {
            dos_full_path_name =
                dynprintf("%s%s/", dir_name ? dir_name : "", vfat_or_dos_name);

            vfat_full_path_name =
                dynprintf("%s%s/", dir_lower_name ? dir_lower_name : "",
                          vfat_filename);
        } else {
            dos_full_path_name =
                dynprintf("%s%s", dir_name ? dir_name : "", vfat_or_dos_name);

            vfat_full_path_name =
                dynprintf("%s%s", dir_lower_name ? dir_lower_name : "",
                          vfat_filename);
        }

        boolean matched;

        if (*vfat_filename) {
            matched = dos_file_match(find, vfat_full_path_name,
                                     dirent_is_dir(dirent));
        } else {
            matched = dos_file_match(find, dos_full_path_name,
                                     dirent_is_dir(dirent));
        }

        vfat_filename[0] = '\0';

        myfree(vfat_or_dos_name);
        myfree(dos_full_path_name);
        myfree(vfat_full_path_name);

        if (matched) {
            found = true;

            DBG("%s exists, do not add in dir %s", find, dir_lower_name);
            break;
        }
    }

    myfree(dir_lower_name);

    return (found);
}

/*
 * dirent_in_use
 *
 * Are sufficent slots free for a filename?
 */
static boolean dirent_in_use (disk_t *disk, fat_dirent_t *dirent,
                              uint32_t slots)
{
    uint32_t i;

    for (i = 0; i < slots; i++) {
        /*
         * Invalid name?
         */
        if (dirent->name[0] == 0x00) {
            dirent++;
            continue;
        }

        /*
         * Deleted file.
         */
        if (dirent->name[0] == FAT_FILE_DELETE_CHAR) {
            dirent++;
            continue;
        }

        return (true);
    }

    return (false);
}

/*
 * dirent_total_sectors
 *
 * How many sectors in all the cluster chains of this directory.
 */
static uint32_t dirent_total_sectors (disk_t *disk, uint32_t cluster)
{
    uint32_t next_cluster;
    uint32_t sectors;
    uint32_t index;

    sectors = 0;
    index = 0;

    for (;;) {
        if (cluster == 0) {
            if (fat_type(disk) == 32) {
                sectors += disk->mbr->sectors_per_cluster;
            } else {
                sectors += root_dir_size_sectors(disk);
            }
        } else {
            sectors += disk->mbr->sectors_per_cluster;
        }

        next_cluster = cluster_next(disk, cluster);

        if (cluster_endchain(disk, next_cluster)) {
            break;
        }

        index++;

        if (index >= MAX_DIRENT_BLOCK) {
            DIE("too many directory chains, %u", index);
            break;
        }

        cluster = next_cluster;
    }

    return (sectors);
}

/*
 * dirents_alloc
 *
 * Allocate a contiguous block of memory with all dirents in it.
 */
static dirent_t *dirents_alloc (disk_t *disk, uint32_t cluster)
{
    uint32_t index;
    uint32_t next_cluster;
    uint32_t sector;
    uint32_t sectors;
    uint8_t *sectordata;
    uint32_t datalen;
    uint8_t *data;
    dirent_t *d;

    d = (typeof(d)) myzalloc(sizeof(*d), __FUNCTION__);
    d->cluster = cluster;

    /*
     * Allocate the contiguous block.
     */
    sectors = dirent_total_sectors(disk, cluster);
    if (!sectors) {
        DIE("zero sized dirent");
    }

    d->dirents =
        (typeof(d->dirents))
        myzalloc(sector_size(disk) * sectors, __FUNCTION__);

    data = (uint8_t*) d->dirents;

    index = 0;

    for (;;) {
        /*
         * Which cluster are we looking at.
         */
        if (cluster == 0) {
            if (fat_type(disk) == 32) {
                /*
                 * FAT32 starts at the root cluster.
                 */
                cluster = disk->mbr->fat.fat32.root_cluster;
                sector = cluster_to_sector(disk, cluster - 2);
                sectors = disk->mbr->sectors_per_cluster;
            } else {
                /*
                 * FAT12/16 have no root cluster.
                 */
                sector = sector_root_dir(disk);
                sectors = root_dir_size_sectors(disk);
            }
        } else {
            sector = cluster_to_sector(disk, cluster - 2);
            sectors = disk->mbr->sectors_per_cluster;
        }

        d->sector[index] = sector;
        d->sectors[index] = sectors;

        /*
         * Read from the disk.
         */
        datalen = sectors * sector_size(disk);
        d->number_of_dirents += datalen / FAT_DIRENT_SIZE;
        d->number_of_chains++;

        sectordata = sector_read(disk, sector, sectors);
        if (!sectordata) {
            DIE("Failed to read sectors whilst reading block of dirents");
        }

        /*
         * Copy into the contiguous block.
         */
        memcpy(data, sectordata, datalen);
        data += datalen;

        myfree(sectordata);

        next_cluster = cluster_next(disk, cluster);

        if (cluster_endchain(disk, next_cluster)) {
            break;
        }

        cluster = next_cluster;

        index++;
        if (index >= MAX_DIRENT_BLOCK) {
            ERR("too many directory chains");
            break;
        }
    }

    return (d);
}

/*
 * dirents_write
 *
 * Write all dirents back to disk.
 */
static void dirents_write (disk_t *disk, dirent_t *d)
{
    uint32_t index;
    uint32_t sector;
    uint32_t sectors;
    uint8_t *data;
    uint32_t datalen;

    if (!d->modified) {
        return;
    }

    data = (uint8_t*) d->dirents;

    for (index = 0; index < d->number_of_chains; index++) {
        sector = d->sector[index];
        sectors = d->sectors[index];
        datalen = sectors * sector_size(disk);

        if (!sector_write(disk, sector, data, sectors)) {
            DIE("cannot write dirent at sector %" PRIu32 "", sector);
        }

        data += datalen;
    }

    d->modified = false;
}

/*
 * dirents_free
 *
 * Free a contiguous block of dirent memory
 */
static void dirents_free (disk_t *disk, dirent_t *d)
{
    dirents_write(disk, d);
    myfree(d->dirents);
    myfree(d);
}

/*
 * dirents_grow
 *
 * Slap a new cluster onto a dirent block
 */
static boolean dirents_grow (disk_t *disk, const dirent_t *d)
{
    uint32_t next_cluster;
    uint32_t new_cluster;
    uint32_t cluster;
    uint32_t index;

    new_cluster = cluster_alloc(disk);
    if (!new_cluster) {
        DIE("Out of clusters/disk space when trying to grow directory");
    }

    uint8_t *data = (typeof(data))
                    myzalloc(cluster_size(disk), __FUNCTION__);

    if (!cluster_write(disk, new_cluster - 2, data, 1)) {
        DIE("cannot grow dirent with empty cluster %" PRIu32 "", new_cluster);
    }

    myfree(data);

    next_cluster = new_cluster;

    cluster = d->cluster;
    index = 0;

    for (;;) {
        next_cluster = cluster_next(disk, cluster);

        if (cluster_endchain(disk, next_cluster)) {
            /*
             * Reached the end of the chain.
             */
            cluster_next_set(disk, cluster, new_cluster,
                             false /* update FAT */);
            cluster_next_set(disk, new_cluster, cluster_max(disk),
                             false /* update FAT */);

            return (true);
        }

        index++;

        if (index >= MAX_DIRENT_BLOCK) {
            ERR("Failed to grow directory, too many directory chains");
            break;
        }

        cluster = next_cluster;
    }

    DIE("Failed to grow directory");

    return (false);
}

/*
 * dirent_find_free_space
 *
 * Find x contiugous free dirents.
 */
static fat_dirent_t *dirent_find_free_space (disk_t *disk,
                                             dirent_t *dirents,
                                             uint32_t slots)
{
    fat_dirent_t *dirent;
    uint32_t d;

    for (d = 0; d < dirents->number_of_dirents - slots; d++) {

        dirent = (fat_dirent_t *)
                (((uint8_t*) dirents->dirents) + (d * FAT_DIRENT_SIZE));

        /*
         * Check we have sufficent fragments.
         */
        if (!dirent_in_use(disk, dirent, slots)) {
            return (dirent);
        }
    }

    return (0);
}

/*
 * vfat_fragments
 *
 * How many fragments will a VFAT filename take up.
 */
static uint32_t vfat_fragments (const char *vfat_filename)
{
    if (!strcmp(vfat_filename, "..")) {
        return (0);
    }

    if (!strcmp(vfat_filename, ".")) {
        return (0);
    }

    char *base = mybasename(vfat_filename, __FUNCTION__);

    uint32_t fragments = (uint32_t) strlen(base) / FAT_VFAT_FILENAME_FRAG_LEN;

    if (strlen(base) % FAT_VFAT_FILENAME_FRAG_LEN) {
        fragments++;
    }

    myfree(base);

    return (fragments);
}

/*
 * dirent_read_name
 *
 * Get the filename alone
 */
static char *dirent_read_name (disk_t *disk, fat_dirent_t *dirent,
                               char *vfat_filename)
{
    fat_dirent_long_t *fat_dirent = (fat_dirent_long_t *) dirent;
    char *filename;

    /*
     * Invalid name?
     */
    if (dirent->name[0] == 0x00) {
        *vfat_filename = '\0';
        return (0);
    }

    if (dirent->name[0] == FAT_FILE_DELETE_CHAR) {
        *vfat_filename = '\0';
        return (0);
    }

    /*
     * Save the long name for the next iteration.
     */
    if (fat_dirent->attr == 0x0F) {
        /*
         * Start of a new VFAT entry?
         */
        if (fat_dirent->order & 0x40) {
            *vfat_filename = '\0';
        }

        char tmp[FAT_VFAT_FILENAME_FRAG_LEN + 1];

        tmp[0] = (fat_dirent->first_5[0] & 0x00FF);
        tmp[1] = (fat_dirent->first_5[1] & 0x00FF);
        tmp[2] = (fat_dirent->first_5[2] & 0x00FF);
        tmp[3] = (fat_dirent->first_5[3] & 0x00FF);
        tmp[4] = (fat_dirent->first_5[4] & 0x00FF);

        tmp[5] = (fat_dirent->next_6[0] & 0x00FF);
        tmp[6] = (fat_dirent->next_6[1] & 0x00FF);
        tmp[7] = (fat_dirent->next_6[2] & 0x00FF);
        tmp[8] = (fat_dirent->next_6[3] & 0x00FF);
        tmp[9] = (fat_dirent->next_6[4] & 0x00FF);
        tmp[10] = (fat_dirent->next_6[5] & 0x00FF);

        tmp[11] = (fat_dirent->final_2[0] & 0x00FF);
        tmp[12] = (fat_dirent->final_2[1] & 0x00FF);
        tmp[13] = 0;

        char tmp2[MAX_STR];

        strncpy(tmp2, vfat_filename, MAX_STR);
        strncpy(vfat_filename, tmp, MAX_STR);
        strncat(vfat_filename, tmp2, MAX_STR);

        return (0);
    }

    /*
     * Get the short name
     */
    filename = dirent_name_copy(dirent);

    return (filename);
}

/*
 * dirent_raw_list
 *
 * Print a filename with no noise
 */
static boolean dirent_raw_list (disk_t *disk,
                                fat_dirent_t *dirent,
                                uint32_t depth,
                                char *dos_full_path_name,
                                char *vfat_full_path_name,
                                char *filename,
                                char *vfat_filename)
{
    /*
     * Omit these dirs as they just fill up the screen and are obvious.
     */
    if (!opt_verbose) {
        if (!strcmp(".", filename)) {
            return (false);
        }

        if (!strcmp("..", filename)) {
            return (false);
        }
    }

    /*
     * Print longname.
     */
    if (vfat_filename[0] != '\0') {
        printf("%s\n", vfat_full_path_name);
    } else {
        printf("%s\n", dos_full_path_name);
    }

    return (true);
}

/*
 * dirent_list
 *
 * Print a filename along with attributes.
 */
static boolean dirent_list (disk_t *disk,
                            fat_dirent_t *dirent,
                            uint32_t depth,
                            char *filename,
                            char *vfat_filename)
{
    const char *attrs;

    /*
     * Omit these dirs as they just fill up the screen and are obvious.
     */
    if (!opt_verbose) {
        if (!strcmp(".", filename)) {
            return (false);
        }

        if (!strcmp("..", filename)) {
            return (false);
        }
    }

    /*
     * File attributes
     */
    attrs = dirent_attr_string(dirent);

    printf("%s ", attrs);

    /*
     * File size
     */
    printf("%12d ", dirent->size);
    if (dirent->size > ONE_MEG) {
        printf("%4uM ", dirent->size / (ONE_MEG));
    } else {
        printf("      ");
    }

    /*
     * Print year, date
     */
    printf("%d %s %.02d ",
           1980 + dirent->lm_date.year,
           dirent_month(dirent->lm_date.month),
           dirent->lm_date.day);

    /*
     * Print longname.
     */
    if (vfat_filename[0] != '\0') {
        if (dirent_is_dir(dirent)) {
            strcat(vfat_filename, "/");
        }

        printf("%*s%s\n", depth, "", vfat_filename);
    } else {
        if (dirent_is_dir(dirent)) {
            strcat(filename, "/");
        }

        printf("%*s%s\n", depth, "", filename);
    }

    return (true);
}

/*
 * file_hexdump
 *
 * Dump the contents of a file.
 */
static boolean
file_hexdump (disk_t *disk, const char *filename, fat_dirent_t *dirent)
{
    uint32_t cluster;
    uint8_t *data;
    uint64_t offset;
    uint32_t sector;
    uint8_t *empty_sector;
    uint32_t next_cluster;
    boolean print_block;
    boolean empty_block;
    int64_t size;

    print_block = true;
    empty_block = false;

    /*
     * For checking if a block is truly empty.
     */
    empty_sector = (typeof(empty_sector))
                    myzalloc(sector_size(disk), __FUNCTION__);

    cluster = dirent_first_cluster(dirent);

    if (!cluster) {
        /*
         * Some FAT disks do do this it seems... Invalid?
         */
        DBG("Bad zero start cluster found while dumping %s", filename);
        return (false);
    }

    /*
     * How much to write.
     */
    size = dirent->size;

    while (!cluster_endchain(disk, cluster)) {

        if (size < 0) {
            ERR("Expected end of file as size now %" PRId64
                ", but more clusters "
                "found, cluster %" PRIu32 " while dumping %s",
                size, cluster, filename);
            return (false);
        }

        VER("Cluster %" PRIu32 " (%s):", cluster, filename);

        data = cluster_read(disk, cluster - 2, 1);
        if (!data) {
            ERR("Failed to read cluster %" PRIu32 " for hex dump", cluster);
                return (false);
        }

        /*
         * Get the address on the disk of where this really is.
         */
        sector = sector_first_data_sector(disk) +
                        ((cluster - 2) * disk->mbr->sectors_per_cluster);

        offset = sector_size(disk) * sector;

        /*
         * Do not print contiguous empty blocks.
         */
        print_block = true;

        if (!memcmp(data, empty_sector, sector_size(disk))) {
            if (empty_block) {
                print_block = false;
            }

            empty_block = true;
        } else {
            empty_block = false;
        }

        if (print_block) {
            /*
             * If the block ended empty, make sure and not to print the
             * next block if also empty.
             */
            if (!disk_hex_dump(disk, data, offset,
                               min(size, cluster_size(disk)))) {
                empty_block = true;
            }
        }

        size -= cluster_size(disk);

        myfree(data);

        next_cluster = cluster_next(disk, cluster);

        if (!next_cluster) {
            ERR("Bad next cluster %" PRIu32 " for cluster %" PRIu32
                " found while dumping %s",
                next_cluster, cluster, filename);
            return (false);
        }

        cluster = next_cluster;

        if (!strcmp(filename, ".") || !strcmp(filename, "..")) {
            /*
             * These next clusters lead us in a loop.
             */
            break;
        }
    }

    if (size > 0) {
        DIE("Premature end of file detected. There are size %" PRIu64 
            " bytes left over and not read from clusters. "
            "Cluster size is %d bytes, so looks "
            "like %" PRIu64 " clusters "
            "are missing from this corrupted file", 
            size,
            cluster_size(disk),
            size / cluster_size(disk));
    }

    myfree(empty_sector);

    return (true);
}

/*
 * file_cat
 *
 * Dump the contents of a file.
 */
static boolean
file_cat (disk_t *disk, const char *filename, fat_dirent_t *dirent)
{
    uint32_t cluster;
    uint8_t *data;
    uint64_t offset;
    uint32_t sector;
    uint8_t *empty_sector;
    uint32_t next_cluster;
    int64_t size;

    /*
     * For checking if a block is truly empty.
     */
    empty_sector = (typeof(empty_sector))
                    myzalloc(sector_size(disk), __FUNCTION__);

    cluster = dirent_first_cluster(dirent);

    /*
     * How much to write.
     */
    size = dirent->size;

    if (!cluster) {
        /*
         * Some FAT disks do do this it seems... Invalid?
         */
        DBG("Bad zero start cluster found while catting %s", filename);
        return (false);
    }

    while (!cluster_endchain(disk, cluster)) {

        if (size < 0) {
            ERR("Expected end of file as size now %" PRId64
                ", but more clusters "
                "found, cluster %" PRIu32 " while dumping %s",
                size, cluster, filename);
            return (false);
        }

        VER("Cluster %" PRIu32 " (%s):", cluster, filename);

        data = cluster_read(disk, cluster - 2, 1);
        if (!data) {
            ERR("Failed to read cluster %" PRIu32 " for hex dump", cluster);
                return (false);
        }

        /*
         * Get the address on the disk of where this really is.
         */
        sector = sector_first_data_sector(disk) +
                        ((cluster - 2) * disk->mbr->sectors_per_cluster);

        offset = sector_size(disk) * sector;

        disk_cat(disk, data, offset, min(size, cluster_size(disk)));

        size -= cluster_size(disk);

        myfree(data);

        next_cluster = cluster_next(disk, cluster);

        if (!next_cluster) {
            ERR("Bad next cluster %" PRIu32 " for cluster %" PRIu32
                " found while dumping %s",
                next_cluster, cluster, filename);
            return (false);
        }

        cluster = next_cluster;

        if (!strcmp(filename, ".") || !strcmp(filename, "..")) {
            /*
             * These next clusters lead us in a loop.
             */
            break;
        }
    }

    if (size > 0) {
        DIE("Premature end of file detected. There are size %" PRIu64 
            " bytes left over and not read from clusters. "
            "Cluster size is %d bytes, so looks "
            "like %" PRIu64 " clusters "
            "are missing from this corrupted file", 
            size,
            cluster_size(disk),
            size / cluster_size(disk));
    }

    myfree(empty_sector);

    return (true);
}

/*
 * file_extract
 *
 * Write a file and contents to disk.
 */
static boolean file_extract (disk_t *disk, const char *filename,
                             fat_dirent_t *dirent)
{
    uint32_t cluster;
    uint8_t *data;
    mode_t mask = getumask();
    int64_t size;

    cluster = dirent_first_cluster(dirent);

    unlink(filename);

    int fd = open(filename, O_CREAT|O_WRONLY, mask);

    if (!fd) {
        ERR("File extract, failed fo mkdir [%s], error: %s",
            filename, strerror(errno));
    }

    if (!opt_quiet) {
        printf("%-55s", filename);
        fflush(stdout);
    }

    /*
     * How much to write.
     */
    size = dirent->size;

    uint32_t last_ok_cluster = 0;

    while (!cluster_endchain(disk, cluster)) {

        VER("Extract cluster %" PRIu32 " (%s) to disk image",
            cluster, filename);

        data = cluster_read(disk, cluster - 2, 1);
        if (!data) {
            ERR("Failed to read cluster %" PRIu32 " for file %s",
                cluster, filename);

            close(fd);
            return (false);
        }

        /*
         * Write this cluster to the real disk.
         */
        if (write(fd, data, min(size, cluster_size(disk))) < 0) {
            DIE("Failed to write cluster %" PRIu32 " for file %s: %s",
                cluster, filename,
                strerror(errno));

            close(fd);
            return (false);
        }

        size -= cluster_size(disk);

        myfree(data);

        DBG5("Finished cluster %" PRIu32 " (%08X)", cluster, cluster);

        last_ok_cluster = cluster;

        cluster = cluster_next(disk, cluster);

        DBG5("Next     cluster %" PRIu32 " (%08X)", cluster, cluster);

        if (!strcmp(filename, ".") || !strcmp(filename, "..")) {
            /*
             * These next clusters lead us in a loop.
             */
            break;
        }
    }

    if (size > 0) {
        /*
         * Disk corruption. Try to print some useful info for analyzing the
         * bad clusters.
         */
        if (opt_debug5) {
            DBG5("Last ok  cluster %" PRIu32 " (%08X)", 
                last_ok_cluster, last_ok_cluster);

            int debug_clusters;

            for (debug_clusters = -2; debug_clusters <= 2; debug_clusters++) {

                DBG5("Debug last ok cluster %" PRIu32 " (%08X) + %d", 
                    last_ok_cluster + debug_clusters, 
                    last_ok_cluster + debug_clusters,
                    debug_clusters);

                uint8_t *data = 
                    cluster_read(disk, last_ok_cluster + debug_clusters, 1);
                hex_dump(data, 0, cluster_size(disk));
                myfree(data);
            }
        }

        DIE("Premature end of file detected. There are size %" PRIu64 
            " bytes left over and not read from clusters. "
            "Cluster size is %d bytes, so looks "
            "like %" PRIu64 " clusters "
            "are missing from this corrupted file. Last cluster was %d.", 
            size,
            cluster_size(disk),
            size / cluster_size(disk),
            cluster);
    }

    if (!opt_quiet) {
        if (dirent->size > ONE_MEG) {
            printf(" %dM", dirent->size / (ONE_MEG));
        } else if (dirent->size > ONE_K) {
            printf(" %dK", dirent->size / ONE_K);
        } else {
            printf(" %" PRIu32 " bytes", dirent->size);
        }

        printf("\n");
    }

    close(fd);

    return (true);
}

/*
 * dir_extract
 *
 * Create a directory recursively while exporting.
 */
static uint32_t dir_extract (disk_t *disk,
                             fat_dirent_t *dirent,
                             const char *fullname,
                             disk_walk_args_t *args)
{
    char *base = mybasename(fullname, __FUNCTION__);
    mode_t mask = getumask();

    if (!strcmp(base, ".")) {
        myfree(base);
        return (0);
    }

    if (!strcmp(base, "..")) {
        myfree(base);
        return (0);
    }

    int rc = 0;

    if (dirent_is_dir(dirent)) {
        if (mkpath(fullname, mask)) {
            rc = 1;
        }

        char *dir_lower_name = duplstr(fullname, __FUNCTION__);
        if (mkpath(dir_lower_name, mask)) {
            rc = 1;
        }

        myfree(dir_lower_name);
    }

    myfree(base);
    return (rc);
}

/*
 * dirent_remove
 *
 * Remove a filename from the disk
 */
static boolean dirent_remove (disk_t *disk,
                              fat_dirent_t *dirent,
                              const char *full_filename,
                              const char *vfat_or_dos_name,
                              const char *vfat_filename)
{
    uint32_t next_cluster;
    uint32_t cluster;

    if (!disk->do_not_output_add_and_remove_while_replacing) {
        if (opt_verbose) {
            if (!opt_quiet) {
                OUT("%-50s removing", full_filename);
                fflush(stdout);
            }
        } else {
            if (strcmp(vfat_or_dos_name, ".") &&
                strcmp(vfat_or_dos_name, "..")) {
                if (!opt_quiet) {
                    OUT("%-50s removing", full_filename);
                    fflush(stdout);
                }
            }
        }
    }

    if (!*vfat_filename) {
        if (strcmp(vfat_or_dos_name, ".") && strcmp(vfat_or_dos_name, "..")) {
            DBG("Warning, %s (%s) has no VFAT filename",
                vfat_or_dos_name, full_filename);
        }
    }

    cluster = dirent_first_cluster(dirent);

    while (!cluster_endchain(disk, cluster)) {
        /*
         * Get the next cluster before we zap the disk.
         */
        next_cluster = cluster_next(disk, cluster);

        /*
         * Mark the FAT so the next cluster points nowhere.
         */
        if (fat_type(disk) == 32) {
            /*
             * Never mark the root cluster as free. Seems like a bad idea.
             */
            if (cluster == disk->mbr->fat.fat32.root_cluster) {
                cluster_next_set(disk, cluster, cluster_max(disk),
                                false /* update FAT */);
            } else {
                cluster_next_set(disk, cluster, 0, false /* update FAT */);
            }
        } else {
            cluster_next_set(disk, cluster, 0, false /* update FAT */);
        }

        cluster = next_cluster;
    }

    /*
     * If a long filename precedes, zap all VFAT fragments.
     */
    if (*vfat_filename) {
        uint32_t fragments = vfat_fragments(vfat_filename);

        int32_t fragment = fragments;

        while (fragment--) {
            fat_dirent_long_t *fat_dirent =
                            (fat_dirent_long_t *) dirent - fragment - 1;

            if (fat_dirent->attr != 0x0F) {
                hex_dump(fat_dirent, 0, sizeof(*dirent));

                DIE("overwriting something that is not VFAT, "
                    "with %" PRIu32 " fragments for %s", fragments,
                    vfat_filename);
            }

#ifdef DEBUG_DIRENT_REMOVE
            hex_dump(fat_dirent, 0, sizeof(*dirent));
#endif
            memset(fat_dirent, 0, sizeof(*fat_dirent));

            fat_dirent_t *d = dirent - fragment - 1;

            d->name[0] = FAT_FILE_DELETE_CHAR;
        }
    }

#ifdef DEBUG_DIRENT_REMOVE
    hex_dump(dirent, 0, sizeof(*dirent));
#endif

    /*
     * We have no cluster for this file now.
     */
    dirent->h_first_cluster = 0;
    dirent->l_first_cluster = 0;

    /*
     * Mark our file as gone.
     */
    dirent->name[0] = FAT_FILE_DELETE_CHAR;

    return (true);
}

/*
 * dos_last_dot
 *
 * Find the last . in the name.
 */
static char *dos_last_dot (char *in)
{
    size_t len = strlen(in);

    while (len--) {
        if (in[len] == '.') {
            return (in + len);
        }
    }

    return (0);
}

/*
 * dos_file_match_ignore_spaces
 *
 * Do a DOS file comparison.
 */
static boolean
dos_file_match_ignore_spaces (const char *a, const char *b, boolean is_dir)
{
    char A[MAX_STR];
    char B[MAX_STR];

    /*
     * Chop off leading spaces and try a match.
     */
    if (a) {
        strncpy(A, a, MAX_STR);
        strchop(A);
    }

    if (b) {
        strncpy(B, b, MAX_STR);
        strchop(B);
    }

    boolean matched;

    matched = file_match(a ? A : 0, b ? B : 0, is_dir);

    return (matched);
}

/*
 * dos_file_match_ignore_case
 *
 * Do a DOS file comparison, ignore case.
 */
static boolean dos_file_match_ignore_case (const char *a, const char *b,
                                           boolean is_dir)
{
    boolean matched = dos_file_match_ignore_spaces(a, b, is_dir);

    /*
     * Match on lower case, DOS does not care.
     */
    if (!matched) {
        char *la = 0;
        char *lb = 0;

        if (a) {
            la = duplstr(a, __FUNCTION__);
        }

        if (b) {
            lb = duplstr(b, __FUNCTION__);
        }

        matched = dos_file_match_ignore_spaces(la, lb, is_dir);

        if (la) {
            myfree(la);
        }

        if (lb) {
            myfree(lb);
        }
    }

    return (matched);
}

/*
 * dos_file_match_include_slash
 *
 * Try with a trailing slash on b.
 */
static boolean dos_file_match_include_slash (const char *a,
                                             const char *b,
                                             boolean is_dir)
{
    boolean matched = dos_file_match_ignore_case(a, b, is_dir);

    /*
     * Try with a trailing slash on one.
     */
    if (!matched) {
        char *lb = 0;

        if (b) {
            lb = dynprintf("%s/", b);
        }

        matched = dos_file_match_ignore_case(a, lb, is_dir);

        if (lb) {
            myfree(lb);
        }
    }

    DBG2("Filter: File match [%s] [%s] matched %d", a, b, matched);

    /*
     * Try with a leading slash on one.
     */
    if (!matched) {
        char *lb = 0;

        if (b) {
            lb = dynprintf("/%s", b);
        }

        matched = dos_file_match_ignore_case(a, lb, is_dir);

        if (lb) {
            myfree(lb);
        }

        DBG2("Filter: File match [%s] [%s] matched %d", a, lb, matched);
    }

    return (matched);
}

/*
 * dos_file_match
 * Do a DOS file comparison, ignore case.
 */
static boolean dos_file_match (const char *a, const char *b, boolean is_dir)
{
    boolean matched = dos_file_match_include_slash(a, b, is_dir);

    return (matched);
}

/*
 * dos_dir_is_subset_of_dir
 *
 * Is a contained within b completely?
 */
boolean dos_dir_is_subset_of_dir (const char *a, const char *b)
{
    char *copya = dupstr(a, __FUNCTION__);
    char *pa;
    char *sa;
    char *copyb = dupstr(b, __FUNCTION__);
    char *pb;
    char *sb;

    pa = copya;
    pb = copyb;

    /*
     * Treat a/b/c and /a/b/c as the same for importing.
     */
    if (*b == '/') {
        pb++;
    }

    for (;;) {
        char *tmpa = dupstr(pa, __FUNCTION__);
        char *tmpb = dupstr(pb, __FUNCTION__);

        sa = strchr(tmpa, '/');
        if (sa) {
            *sa = 0;
        }

        sb = strchr(tmpb, '/');
        if (sb) {
            *sb = 0;
        }

        if (!dos_file_match(tmpa, tmpb, false)) {
            myfree(tmpa);
            myfree(tmpb);
            myfree(copya);
            myfree(copyb);
            return (false);
        }

        myfree(tmpa);
        myfree(tmpb);

        sa = strchr(pa, '/');
        sb = strchr(pb, '/');

        if (!sa || !sb) {
            break;
        }

        pa = sa + 1;
        pb = sb + 1;
    }

    myfree(copya);
    myfree(copyb);

    return (true);
}

/*
 * The main directory walker. Walk dirs, creatig, printing, deleting files...
 */
static uint32_t disk_walk_ (disk_t *disk,
                            const char *filter,
                            const char *dir_name,
                            uint32_t cluster,
                            uint32_t parent_cluster,
                            uint32_t depth,
                            disk_walk_args_t *args)
{
    char *vfat_or_dos_name;
    uint32_t next_cluster;
    fat_dirent_t *dirent;
    dirent_t *dirents;
    char *dir_lower_name;
    char *slash_dir_name;
    uint32_t d;
    uint32_t count;

    DBG2("DIR walk: dir \"%s\" filter \"%s\"", dir_name, filter);

    if (args->stop_walk) {
        return (false);
    }

    if (depth > MAX_DIR_DEPTH) {
        ERR("runaway directory recursion at depth %" PRIu32 ", dir %s",
            depth, dir_name);
    }

    count = false;

    /*
     * Which cluster are we looking at.
     */
    if (fat_type(disk) == 32) {
        /*
         * If FAT32, start from the root cluster.
         */
        if (cluster == 0) {
            parent_cluster = cluster = disk->mbr->fat.fat32.root_cluster;
        }
    }

    dirents = dirents_alloc(disk, cluster);
    if (!dirents) {
        return (false);
    }

    dirent = dirents->dirents;

    if (!dir_name) {
        dir_name = "/";
    }

    /*
     * Keep a lower case copy of the name for regexp matching.
     */
    dir_lower_name = duplstr(dir_name, __FUNCTION__);

    if (*dir_name != '/') {
        slash_dir_name = dynprintf("/%s", dir_name);
    } else {
        slash_dir_name = dupstr(dir_name, __FUNCTION__);
    }

    /*
     * Read all files in the dir.
     */
    boolean found_dot_dot_dir = false;
    boolean found_dot_dir = false;

    char vfat_filename[MAX_STR];

    vfat_filename[0] = '\0';

    for (d = 0; d < dirents->number_of_dirents; d++) {

        if (args->stop_walk) {
            break;
        }

        dirent = (fat_dirent_t *)
                        (((uint8_t*) dirents->dirents) + (d * FAT_DIRENT_SIZE));

        if (opt_debug3) {
            if (dirent_in_use(disk, dirent, 1)) {
                hex_dump(dirent, 0, sizeof(*dirent));
            }
        }

        vfat_or_dos_name = dirent_read_name(disk, dirent, vfat_filename);
        if (!vfat_or_dos_name) {
            continue;
        }

        DBG2("Filter file \"%s\"", vfat_or_dos_name);

        strchop(vfat_filename);

        DBG3("DIR %s FILE %s (%s)", dir_name, vfat_filename, vfat_or_dos_name);

        /*
         * Sanity check the dir is sane.
         */
        if (!strcmp(vfat_or_dos_name, ".")) {
            if (found_dot_dir) {
                ERR("found 2nd . dir in dir %s", dir_name);
            }

            found_dot_dir = true;
        }

        if (!strcmp(vfat_or_dos_name, ".")) {
            if (found_dot_dot_dir) {
                ERR("found 2nd .. dir in dir %s", dir_name);
            }

            found_dot_dot_dir = true;
        }

        char *dos_full_path_name;
        char *vfat_full_path_name;

        if (dirent_is_dir(dirent)) {
            dos_full_path_name =
                dynprintf("%s%s/", dir_name ? dir_name : "", vfat_or_dos_name);
            vfat_full_path_name =
                dynprintf("%s%s/", dir_lower_name ? dir_lower_name : "",
                          *vfat_filename ? vfat_filename: vfat_or_dos_name);
        } else {
            dos_full_path_name =
                dynprintf("%s%s", dir_name ? dir_name : "", vfat_or_dos_name);
            vfat_full_path_name =
                dynprintf("%s%s", dir_lower_name ? dir_lower_name : "",
                          vfat_filename);
        }
        boolean matched;

        if (*vfat_filename) {
            matched = dos_file_match(filter, vfat_full_path_name,
                                     dirent_is_dir(dirent));
        } else {
            matched = dos_file_match(filter, dos_full_path_name,
                                     dirent_is_dir(dirent));
        }

        /*
         * For listing and removing, what we print.
         */
        char *output_name = dos_full_path_name;

        if (*vfat_full_path_name) {
            if (strlen(vfat_full_path_name) >= strlen(dos_full_path_name)) {
                output_name = vfat_full_path_name;
            }
        }

        /*
         * Raw list a file.
         */
        if (matched && args->find && args->print) {
            count += dirent_raw_list(disk, dirent, depth, 
                                     dos_full_path_name,
                                     vfat_full_path_name,
                                     vfat_or_dos_name,
                                     vfat_filename);
        /*
         * List a file.
         */
        } else if (matched && args->print) {
            count += dirent_list(disk, dirent, depth, vfat_or_dos_name,
                                 vfat_filename);
        }

        /*
         * Hexdump a file.
         */
        if (matched && args->hexdump) {
            count += dirent_list(disk, dirent, depth, vfat_or_dos_name,
                                 vfat_filename);

            file_hexdump(disk, vfat_or_dos_name, dirent);
        }

        /*
         * Cat a file.
         */
        if (matched && args->cat) {
            file_cat(disk, vfat_or_dos_name, dirent);
        }

        /*
         * Extract a file.
         */
        if (matched && args->extract &&
            !dos_file_match(vfat_or_dos_name, ".", true) &&
            !dos_file_match(vfat_or_dos_name, "..", true)) {

            count += dir_extract(disk, dirent, vfat_full_path_name, args);

            if (!dirent_is_dir(dirent)) {
                count += file_extract(disk, output_name, dirent);
            }
        }

        if (matched && args->find) {
            if (!args->walk_whole_tree) {
                args->stop_walk = true;
                args->dirent = *dirent;
            }

            count++;
        }

        /*
         * If a dir, recurse.
         */
        if (!args->stop_walk && dirent_is_dir(dirent)) {
            next_cluster = dirent_first_cluster(dirent);

            DBG2("Filter enter \"%s\"", vfat_or_dos_name);

            DBG3("  [%s] is a dir next cluster %u, 0x%x",
                 vfat_or_dos_name,
                 next_cluster, next_cluster);

            if (!cluster_endchain(disk, next_cluster) &&
                !dos_file_match(vfat_or_dos_name, ".", true) &&
                !dos_file_match(vfat_or_dos_name, "..", true) &&
                (next_cluster != cluster)) {

                /*
                 * If this directory path fragment is not in the full path we 
                 * want to look for then don't waste time searching.
                 */
                boolean enter_subdir = false;

                if (filter) {
                    if (strisregexp(filter)) {
                        enter_subdir = true;
                    } else {
                        enter_subdir =
                            dos_dir_is_subset_of_dir(vfat_full_path_name, filter);
                    }
                } else {
                    enter_subdir = true;
                }

                if (enter_subdir) {
                    /*
                     * If a filter matches a parent dir then it matches all
                     * child dirs.
                     */
                    const char *subdir_filter;

                    if (matched) {
                        subdir_filter = 0;
                    } else {
                        subdir_filter = filter;
                    }

                    DBG2("Enter subdir \"%s\", cluster %" PRIu32 
                         " -> %" PRIu32 " %s",
                         vfat_or_dos_name, cluster,
                         next_cluster, vfat_full_path_name);

                    count += disk_walk(disk,
                                       subdir_filter,
                                       vfat_full_path_name,
                                       next_cluster, /* new cluster */
                                       cluster,  /* parent cluster now */
                                       depth + 1, args);
                }
            }
        }

        /*
         * Remove a file.
         */
        if (matched && args->remove) {
            if (dirent_remove(disk, dirent, output_name, vfat_or_dos_name,
                              vfat_filename)) {

                dirents->modified = true;

                if (!dirent_is_dir(dirent)) {
                    count++;
                }
            }
        }

        myfree(vfat_or_dos_name);
        myfree(dos_full_path_name);
        myfree(vfat_full_path_name);

        vfat_filename[0] = '\0';

        if (!args->walk_whole_tree) {
            if (matched && args->find) {
                break;
            }
        }
    }

    /*
     * See if this is a dir we want to create or add a file or dir inside.
     */
    if (args->add) {
        boolean add_here = false;

        if (args->add && (depth == 0) && !strcmp(args->add_dir, "/")) {
            add_here = true;
        }

        if (args->add) {
            char *add_dir_name =
                dynprintf("%s/", args->add_dir ? args->add_dir : "");

            if (!strcasecmp(add_dir_name, dir_name) ||
                !strcasecmp(add_dir_name, slash_dir_name)) {
                add_here = true;
            }

            myfree(add_dir_name);
        }

        /*
         * Check the file or dir does not already exist.
         */
        if (add_here) {
            if (dirent_entry_exists(disk, dirents, dir_name, filter)) {
                add_here = false;
            }
        }

        /*
         * If we still want to add the file or dir, now try and find enough
         * free slots in the dirents.
         */
        if (add_here) {
            uint32_t fragments = vfat_fragments(filter);

            /*
             * Try to find space.
             */
            dirent = NULL;

            for (;;) {
                dirent = dirent_find_free_space(disk, dirents, fragments + 1);
                if (dirent) {
                    break;
                }

                DBG2("Dirent grow needed for %s for %u fragments for \"%s\"",
                     dir_name, fragments + 1, filter);

                if (!dirents_grow(disk, dirents)) {
                    goto cleanup;
                }

                /*
                 * Lazy, but just reread the disk.
                 */
                dirents_free(disk, dirents);
                dirents = dirents_alloc(disk, cluster);

                if (!dirents) {
                    goto cleanup;
                }
            }

            /*
             * Now add the file to the dirent.
             */
            count += file_import(disk, args, dirent, filter, cluster, depth);

            dirents->modified = true;

            args->stop_walk = true;
        }
    }

cleanup:
    dirents_free(disk, dirents);

    myfree(dir_lower_name);
    myfree(slash_dir_name);

    return (count);
}

/*
 * disk_walk
 *
 * Wrapper for the big disk walker.
 */
uint32_t disk_walk (disk_t *disk,
                    const char *filter_,
                    const char *dir_name_,
                    uint32_t cluster,
                    uint32_t parent_cluster,
                    uint32_t depth,
                    disk_walk_args_t *args)
{
    char *filter = filter_ ? filename_cleanup(filter_) : 0;
    char *dir_name = dir_name_ ? filename_cleanup(dir_name_) : 0;

    uint32_t ret;

    ret = disk_walk_(disk, filter, dir_name, cluster, parent_cluster,
                     depth, args);

    if (filter) {
        myfree(filter);
    }

    if (dir_name) {
        myfree(dir_name);
    }

    return (ret);
}

/*
 * fat_format
 *
 * Format the FAT on a new disk.
 */
boolean fat_format (disk_t *disk, uint32_t partition, uint32_t os_id)
{
    uint8_t *sector0 = (uint8_t *)disk->mbr;

    switch (os_id) {
        case DISK_FAT12:
        case DISK_FAT16:
        case DISK_FAT16_LBA:
        case DISK_FAT32:
        case DISK_FAT32_LBA:
            sector0[opt_sector_size - 2] = 0x55;
            sector0[opt_sector_size - 1] = 0xAA;
            break;
    default:
        DIE("Not a FAR OS ID %u", os_id);
    }

    /*
     * FAT signature in the second sector.
     */
    fat_fsinfo *fsinfo = (typeof(fsinfo)) (sector0 + opt_sector_size);

    fsinfo->signature1[3] = 0x41;
    fsinfo->signature1[2] = 0x61;
    fsinfo->signature1[1] = 0x52;
    fsinfo->signature1[0] = 0x52;
    fsinfo->signature2[3] = 0x61;
    fsinfo->signature2[2] = 0x41;
    fsinfo->signature2[1] = 0x72;
    fsinfo->signature2[0] = 0x72;

    switch (os_id) {
        case DISK_FAT12:
            if (fat_type(disk) != 12) {
                DIE("too many clusters, %" PRIu32 " specified for fat 12, "
                    "must be < 4085. Try a smaller disk size.",
                    total_clusters(disk));
            }
            break;

        case DISK_FAT16:
        case DISK_FAT16_LBA:
            if (fat_type(disk) != 16) {
                DIE("too many clusters, %" PRIu32 " specified for fat 16, "
                    "must be < 65525. Try a smaller disk size.",
                    total_clusters(disk));
            }
            break;

        case DISK_FAT32:
        case DISK_FAT32_LBA:
            if (fat_type(disk) != 32) {
                DIE("too few clusters, %" PRIu32 " specified for fat 32, "
                    "must be >= 65525. Try a larger disk size.",
                    total_clusters(disk));
            }
            break;
    }

    if (!opt_quiet) {
        OUT("  Zeroing root dir sectors...");
    }

    /*
     * Zap the root dir so it is empty.
     */
    uint8_t *empty_sector;
    uint32_t sector;

    empty_sector = (typeof(empty_sector))
                    myzalloc(sector_size(disk), __FUNCTION__);
    memset(empty_sector, 0x00, sector_size(disk));

    for (sector = sector_root_dir(disk);
         sector < sector_root_dir(disk) + ONE_K;
         sector++) {

        sector_write(disk, sector, empty_sector, 1);
    }

    myfree(empty_sector);

    if (!opt_quiet) {
        OUT("  Creating partition %u FAT %" PRIu32 " filesystem "
            "with %" PRIu32 " clusters",
            disk->partition,
            fat_type(disk),
            total_clusters(disk));
    }

    /*
     * Set up an empty FAT
     */
    uint32_t start = disk->parts[partition]->LBA;
    uint32_t end = start + disk->parts[partition]->sectors_in_partition;

    uint32_t fat_byte_offset;
    uint32_t cluster;

    /*
     * Ensure that our FAT can address the full disk.
     */
    for (;;) {
        cluster = total_clusters(disk);

        if (fat_type(disk) == 12) {
            fat_byte_offset = cluster + (cluster / 2); // multiply by 1.5
        } else if (fat_type(disk) == 16) {
            fat_byte_offset = cluster * sizeof(uint16_t);
        } else if (fat_type(disk) == 32) {
            fat_byte_offset = cluster * sizeof(uint32_t);
        } else {
            DIE("bug");
        }

        DBG("FAT max cluster address offset %u "
            "FAT size in bytes %" PRIu64 " "
            "Total data clusters %u",
            fat_byte_offset, fat_size_bytes(disk), total_clusters(disk));

        /*
         * FAT too small?
         */
        if (fat_byte_offset > fat_size_bytes(disk)) {
            DBG("Increase FAT size");

            if (fat_type(disk) == 12) {
                disk->mbr->fat_size_sectors++;
            } else if (fat_type(disk) == 16) {
                disk->mbr->fat_size_sectors++;
            } else if (fat_type(disk) == 32) {
                disk->mbr->fat.fat32.fat_size_sectors++;
            } else {
                DIE("bug");
            }
        } else {
            break;
        }
    }

    /*
     * Read the null fat.
     */
    fat_read(disk);

    if (!disk->fat) {
        ERR("no FAT read from disk");
        return (false);
    }

    for (cluster = 0; cluster < total_clusters(disk); cluster++) {

        sector = cluster_to_sector(disk, cluster - 2);
        if (sector >= end) {
            DIE("Attempt to write to FAT beyond end of disk at sector %u "
                "disk range, start %u end %u",
                sector, start, end);
        }

        /*
         * Very important here to have cluster 2, the root dir not marked
         * as empty space as we skip it when allocating clusters and we
         * would end up writing file data onto it instead.
         */
        if (cluster <= 2) {
            cluster_next_set(disk, cluster, cluster_max(disk),
                             false /* update FAT */);
        } else {
            cluster_next_set(disk, cluster, 0, false /* update FAT */);
        }
    }

    return (true);
}

/*
 * do_disk_command_add_file_or_dir_in
 *
 * Add a single file or dir in the given directory.
 */
static uint32_t
do_disk_command_add_file_or_dir_in (disk_t *disk,
                                    char *source,
                                    const char *parent_dir,
                                    char *file_or_dir_,
                                    boolean is_intermediate_dir)
{
    char *file_or_dir;
    uint32_t count;

    disk_walk_args_t args = {0};
    args.add = true;
    args.is_intermediate_dir = is_intermediate_dir;

    if (!strcmp(parent_dir, ".")) {
        parent_dir = "/";
    }

    args.add_dir = filename_cleanup(parent_dir);
    args.source = dupstr(source, __FUNCTION__);
    file_or_dir = filename_cleanup(file_or_dir_);

    if (disk->do_not_output_add_and_remove_while_replacing) {
        char *base = mybasename(file_or_dir, __FUNCTION__);

        if (!opt_quiet) {
            OUT("%-50s replacing in %s", base, args.add_dir);
            fflush(stdout);
        }

        myfree(base);
    } else {
        char *base = mybasename(file_or_dir, __FUNCTION__);

        if (!opt_quiet) {
            OUT("%-50s adding in %s", base, args.add_dir);
            fflush(stdout);
        }

        myfree(base);
    }

    count = disk_walk(disk, file_or_dir, "", 0, 0, 0, &args);

    myfree(args.add_dir);
    myfree(args.source);
    myfree(file_or_dir);

    return (count);
}

/*
 * do_disk_command_add_file_or_dir
 *
 * Add a single file or dir
 */
static uint32_t
do_disk_command_add_file_or_dir (disk_t *disk,
                                 char *source,
                                 char *target,
                                 boolean is_intermediate_dir)
{
    uint32_t count;

    count = 0;

    if (!source || dir_exists(source)) {
        source = target;
    } else {
        if (!file_exists(source)) {
            ERR("Failed to read file %s for importing", source);
            return (0);
        }
    }

    if (!strcmp(source, ".")) {
        return (0);
    }

    disk_walk_args_t args = {0};
    args.find = true;

    if (disk_walk(disk, target, "", 0, 0, 0, &args)) {
        if (dirent_is_dir(&args.dirent)) {
            if (dir_exists(target)) {
                VER("%s dir exists", target);
                return (0);
            }
        } else {
            if (!opt_verbose) {
                disk->do_not_output_add_and_remove_while_replacing = true;
            }

            disk_walk_args_t args = {0};
            args.remove = true;
            count = disk_walk(disk, target, "", 0, 0, 0, &args);
            if (!count) {
                ERR("failed to replace %s\n", target);
                return (0);
            }
        }
    }

    char *tmp = dupstr(target, __FUNCTION__);

    count = do_disk_command_add_file_or_dir_in(disk, source, dirname(tmp), 
                                               target, is_intermediate_dir);

    myfree(tmp);

    return (count);
}

/*
 * disk_command_add_file_or_dir
 *
 * Add a single file or dir, adding all paths first
 */
uint32_t
disk_command_add_file_or_dir (disk_t *disk,
                              const char *source_file_or_dir,
                              const char *target_file_or_dir,
                              boolean addfile)
{
    char *source = dupstr(source_file_or_dir, __FUNCTION__);
    char *target = filename_cleanup(target_file_or_dir);
    char *copypath = dupstr(target, __FUNCTION__);
    boolean status;
    uint32_t count;
    char *pp;
    char *sp;

    /*
     * If this is a path like ../foo ./foo ~/foo, then just take the name
     * of the file for importing as we cannot create the intermediate steps
     * easily within dos.
     */
    if (!addfile) {
        if (!file_exists(target) && !dir_exists(target)) {
            myfree(target);
            target = mybasename(source_file_or_dir, __FUNCTION__);

            myfree(copypath);
            copypath = dupstr(target, __FUNCTION__);
        }
    }

    /*
     * Make sure all paths exist.
     */
    status = true;
    pp = copypath;

    while (status && (sp = strchr(pp, '/')) != 0) {
        if (sp != pp) {
            *sp = '\0';

            do_disk_command_add_file_or_dir(disk, 0, copypath, 
                                            true /* is_intermediate_dir */);

            *sp = '/';
        }

        pp = sp + 1;
    }

    myfree(copypath);

    count = do_disk_command_add_file_or_dir(disk, source, target, 
                                            false /* is_intermediate_dir */);

    myfree(source);
    myfree(target);

    return (count);
}

