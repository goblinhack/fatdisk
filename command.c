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
#include "main.h"

#include "disk.h"
#include "fat.h"
#include "command.h"

/*
 * disk_command_open
 *
 * Read the boot record, partition table, FAT etc...
 */
disk_t *disk_command_open (const char *filename,
                           int64_t offset,
                           uint32_t partition,
                           boolean partition_set)
{
    disk_t *disk;

    disk = (typeof(disk)) myzalloc(sizeof(*disk), __FUNCTION__);

    disk->filename = filename;
    disk->offset = offset;
    disk->partition = partition;
    disk->partition_set = partition_set;

    disk->mbr = (typeof(disk->mbr))
                    disk_read_from(disk, 0, sizeof(*disk->mbr));
    if (!disk->mbr) {
        ERR("File, \"%s\" has no boot record", filename);
        myfree(disk);
        return (0);
    }

    disk->sector0 = (typeof(disk->sector0))
                    disk_read_from(disk, 0, sector_size(disk) * 2);
    if (!disk->sector0) {
        disk_hex_dump(disk, disk->mbr, 0, sizeof(*disk->mbr));

        disk_command_info(disk);

        ERR("File, \"%s\" failed to read boot sector 0 (sector %" 
            PRIu32 ") (%" PRIu32 " bytes) for offset %" PRIx64,
            filename,
            sector_offset(disk),
            sector_size(disk),
            offset);

        myfree(disk);
        return (0);
    }

    if ((disk->sector0[opt_sector_size - 2] != 0x55) ||
        (disk->sector0[opt_sector_size - 1] != 0xAA)) {
        DIE("File, \"%s\" is not a DOS disk, bad signature", filename);
    }

    partition_table_read(disk);

    fat_read(disk);

    return (disk);
}

/*
 * disk_command_info
 *
 * Dump the boot block and partition info
 */
boolean disk_command_info (disk_t *disk)
{
    if (!disk) {
        return (false);
    }

    /*
     * Dump the first sector.
     */
    if (opt_verbose) {
        OUT("Sector 0 (abs %" PRIu32 "), %" PRIu32 " bytes:",
            sector_offset(disk), sector_size(disk));

        if (disk->sector0) {
            disk_hex_dump(disk, disk->sector0, 0, sector_size(disk));
        }
    }

    partition_table_print(disk);

    OUT("Boot record info:");

    OUT("  %*s%02X %02X %02X", -OUTPUT_FORMAT_WIDTH, "boot jmp",
        disk->mbr->bootjmp[0], disk->mbr->bootjmp[1],
        disk->mbr->bootjmp[2]);

    OUT("  %*s%c%c%c%c%c%c%c%c", -OUTPUT_FORMAT_WIDTH, "OEM",
        disk->mbr->oem_id[0], disk->mbr->oem_id[1],
        disk->mbr->oem_id[2], disk->mbr->oem_id[3],
        disk->mbr->oem_id[4], disk->mbr->oem_id[5],
        disk->mbr->oem_id[6], disk->mbr->oem_id[7]);

    if (fat_type(disk) == 32) {
        OUT("  %*s\"%c%c%c%c%c%c%c%c%c%c%c\"", 
            -OUTPUT_FORMAT_WIDTH, "Volume label",
            disk->mbr->fat.fat32.volume_label[0],
            disk->mbr->fat.fat32.volume_label[1],
            disk->mbr->fat.fat32.volume_label[2],
            disk->mbr->fat.fat32.volume_label[3],
            disk->mbr->fat.fat32.volume_label[4],
            disk->mbr->fat.fat32.volume_label[5],
            disk->mbr->fat.fat32.volume_label[6],
            disk->mbr->fat.fat32.volume_label[7],
            disk->mbr->fat.fat32.volume_label[8],
            disk->mbr->fat.fat32.volume_label[9],
            disk->mbr->fat.fat32.volume_label[10]);

        OUT("  %*s\"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\"", 
            -OUTPUT_FORMAT_WIDTH, "Volume label (hex)",
            disk->mbr->fat.fat32.volume_label[0],
            disk->mbr->fat.fat32.volume_label[1],
            disk->mbr->fat.fat32.volume_label[2],
            disk->mbr->fat.fat32.volume_label[3],
            disk->mbr->fat.fat32.volume_label[4],
            disk->mbr->fat.fat32.volume_label[5],
            disk->mbr->fat.fat32.volume_label[6],
            disk->mbr->fat.fat32.volume_label[7],
            disk->mbr->fat.fat32.volume_label[8],
            disk->mbr->fat.fat32.volume_label[9],
            disk->mbr->fat.fat32.volume_label[10]);

        OUT("  %*s%c%c%c%c%c%c%c%c", -OUTPUT_FORMAT_WIDTH, "FAT32 label",
            disk->mbr->fat.fat32.fat_type_label[0],
            disk->mbr->fat.fat32.fat_type_label[1],
            disk->mbr->fat.fat32.fat_type_label[2],
            disk->mbr->fat.fat32.fat_type_label[3],
            disk->mbr->fat.fat32.fat_type_label[4],
            disk->mbr->fat.fat32.fat_type_label[5],
            disk->mbr->fat.fat32.fat_type_label[6],
            disk->mbr->fat.fat32.fat_type_label[7]);
    } else {
        OUT("  %*s\"%c%c%c%c%c%c%c%c%c%c%c\"",
            -OUTPUT_FORMAT_WIDTH, "Volume label",
            disk->mbr->fat.fat16.volume_label[0],
            disk->mbr->fat.fat16.volume_label[1],
            disk->mbr->fat.fat16.volume_label[2],
            disk->mbr->fat.fat16.volume_label[3],
            disk->mbr->fat.fat16.volume_label[4],
            disk->mbr->fat.fat16.volume_label[5],
            disk->mbr->fat.fat16.volume_label[6],
            disk->mbr->fat.fat16.volume_label[7],
            disk->mbr->fat.fat16.volume_label[8],
            disk->mbr->fat.fat16.volume_label[9],
            disk->mbr->fat.fat16.volume_label[10]);

        OUT("  %*s\"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\"",
            -OUTPUT_FORMAT_WIDTH, "Volume label (hex)",
            disk->mbr->fat.fat16.volume_label[0],
            disk->mbr->fat.fat16.volume_label[1],
            disk->mbr->fat.fat16.volume_label[2],
            disk->mbr->fat.fat16.volume_label[3],
            disk->mbr->fat.fat16.volume_label[4],
            disk->mbr->fat.fat16.volume_label[5],
            disk->mbr->fat.fat16.volume_label[6],
            disk->mbr->fat.fat16.volume_label[7],
            disk->mbr->fat.fat16.volume_label[8],
            disk->mbr->fat.fat16.volume_label[9],
            disk->mbr->fat.fat16.volume_label[10]);

        OUT("  %*s\"%c%c%c%c%c%c%c%c\"",
            -OUTPUT_FORMAT_WIDTH, "FAT label",
            disk->mbr->fat.fat16.fat_type_label[0],
            disk->mbr->fat.fat16.fat_type_label[1],
            disk->mbr->fat.fat16.fat_type_label[2],
            disk->mbr->fat.fat16.fat_type_label[3],
            disk->mbr->fat.fat16.fat_type_label[4],
            disk->mbr->fat.fat16.fat_type_label[5],
            disk->mbr->fat.fat16.fat_type_label[6],
            disk->mbr->fat.fat16.fat_type_label[7]);
    }

    OUT("  %*s%" PRIu64 "M", -OUTPUT_FORMAT_WIDTH, "disk size",
        disk_size(disk) / (ONE_MEG));

    OUT("  %*s%" PRIu64 "", -OUTPUT_FORMAT_WIDTH, "disk size (bytes)",
        disk_size(disk));

    OUT("  %*s%" PRIu32 " bytes", -OUTPUT_FORMAT_WIDTH, "sector size",
        sector_size(disk));

    OUT("  %*s%" PRIu32 " bytes", -OUTPUT_FORMAT_WIDTH, "cluster size",
        cluster_size(disk));

    OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "sectors per cluster",
        disk->mbr->sectors_per_cluster);

    OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "sectors per track",
        disk->mbr->sectors_per_track);

    OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "reserved sectors",
        disk->mbr->reserved_sector_count);

    OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "hidden sectors",
        disk->mbr->sectors_hidden);

    OUT("  %*s%" PRIu64 "", -OUTPUT_FORMAT_WIDTH, "total sectors",
        sector_count_total(disk));

    OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "total clusters",
        total_clusters(disk));

    OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "number of heads",
        disk->mbr->nheads);

    OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "FAT copies",
        disk->mbr->number_of_fats);

    OUT("  %*s%" PRIu32 " (FAT12/16 only)", -OUTPUT_FORMAT_WIDTH, 
        "FAT size in sectors",
        disk->mbr->fat_size_sectors);

    OUT("  %*s%" PRIu64 " bytes", -OUTPUT_FORMAT_WIDTH, "FAT size",
        fat_size_bytes(disk));

    OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "FAT sector start",
        sector_reserved_count(disk));

    OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "FAT root dirs",
        disk->mbr->number_of_dirents);

    OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "FAT root dir sector",
        sector_root_dir(disk));

    OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH,
        "FAT root dir size in sectors",
        root_dir_size_sectors(disk));

    OUT("  %*s%" PRIu32 " bytes", -OUTPUT_FORMAT_WIDTH, "FAT root dir size",
        root_dir_size_bytes(disk));

    OUT("  %*s%" PRIu64 "", -OUTPUT_FORMAT_WIDTH, "FAT data sectors",
        sector_count_data(disk));

    OUT("  %*s0x%02X %s", -OUTPUT_FORMAT_WIDTH, "media type",
        disk->mbr->media_type,
        msdos_get_media_type(disk->mbr->media_type));

    if (fat_type(disk) == 32) {
        /*
         * Boot record with FAT32 extension.
         */
        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "FAT32 size in sectors",
            disk->mbr->fat.fat32.fat_size_sectors);

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "FAT32 flags",
            disk->mbr->fat.fat32.extended_flags);

        if (disk->mbr->fat.fat32.extended_flags & 0x80) {
            OUT("(one: single active FAT");

            OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "FAT32 active FATs",
                (disk->mbr->fat.fat32.extended_flags & 7));
        }

        OUT("  %*s%X", -OUTPUT_FORMAT_WIDTH, "FAT32 boot signature",
            disk->mbr->fat.fat32.boot_signature);

        OUT("  %*s0x%08X", -OUTPUT_FORMAT_WIDTH, "FAT32 volume id",
            disk->mbr->fat.fat32.volume_id);

        OUT("  %*s%x", -OUTPUT_FORMAT_WIDTH, "FAT32 extended flags",
            disk->mbr->fat.fat32.extended_flags);

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "FAT version",
            disk->mbr->fat.fat32.fat_version);

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "FAT32 root cluster",
            disk->mbr->fat.fat32.root_cluster);

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "FAT info",
            disk->mbr->fat.fat32.fat_info);

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "FAT32 backup sector",
            disk->mbr->fat.fat32.backup_boot_sector);

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "FAT32 drive number",
            disk->mbr->fat.fat32.drive_number);
    } else {
        /*
         * Boot record with FAT16 extension.
         */
        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH, "FAT16 drive number",
            disk->mbr->fat.fat16.bios_drive_num);

        OUT("  %*s%x", -OUTPUT_FORMAT_WIDTH, "FAT16 boot signature",
            disk->mbr->fat.fat16.boot_signature);

        OUT("  %*s0x%08x", -OUTPUT_FORMAT_WIDTH, "FAT16 volume id",
            disk->mbr->fat.fat16.volume_id);
    }

    if (!disk->sector0) {
        return (false);
    }

    /*
     * Dump the FATs
     */
    uint32_t f;

    for (f = 0; f < disk->mbr->number_of_fats; f++) {
        uint8_t *fat = sector_read(disk, sector_reserved_count(disk) +
                                   f * fat_size_sectors(disk),
                                   fat_size_sectors(disk));
        if (!fat) {
            ERR("Failed to read FAT %" PRIu32 "", f);
            continue;
        }

        OUT("FAT %" PRIu32 ", sector (%" PRIu64 "->%" PRIu64
            ", abs %" PRIu64 "->%" PRIu64 "), %" PRIu64 " bytes", f,
            sector_reserved_count(disk) + f * fat_size_sectors(disk),
            (sector_reserved_count(disk) + (f+1) * fat_size_sectors(disk)) - 1,
            sector_offset(disk) +
                sector_reserved_count(disk) + f * fat_size_sectors(disk),
            sector_offset(disk) +
                (sector_reserved_count(disk) + (f+1) * fat_size_sectors(disk)) - 1,
            fat_size_bytes(disk));

        if (opt_verbose) {
            disk_hex_dump(disk, fat,
                        (sector_reserved_count(disk) * sector_size(disk)) +
                        (f * fat_size_bytes(disk)),
                        128 /* fat_size_bytes(disk) */);
        }

        myfree(fat);
    }

    /*
     * Dump the root dir.
     */
    uint8_t *root_dir_data;

    root_dir_data = sector_read(disk, sector_root_dir(disk),
                                disk->mbr->sectors_per_cluster);
    if (!root_dir_data) {
        ERR("Failed to read root dir cluster");
        return (false);
    }

    if (opt_verbose) {
        OUT("Root dir cluster (sector %" PRIu32 ", abs %" PRIu32 "):",
            sector_root_dir(disk),
            sector_offset(disk) + sector_root_dir(disk));

        disk_hex_dump(disk, root_dir_data, 0, 128 /* cluster_size(disk) */);
    }

    myfree(root_dir_data);

    /*
     * Dump the first cluster.
     */
    uint8_t *cluster_data;

    cluster_data = sector_read(disk, sector_first_data_sector(disk),
                                disk->mbr->sectors_per_cluster);
    if (!cluster_data) {
        ERR("Failed to read root dir cluster");
        return (false);
    }

    if (opt_verbose) {
        OUT("First cluster (sector %" PRIu32 ", abs %" PRIu32 "):",
            sector_first_data_sector(disk),
            sector_offset(disk) + sector_first_data_sector(disk));

        disk_hex_dump(disk, cluster_data, 0, 128 /* cluster_size(disk) */);
    }

    myfree(cluster_data);

    return (true);
}

/*
 * disk_command_close
 *
 * Free up resources and write diry sectors to disk.
 */
void disk_command_close (disk_t *disk)
{
    uint32_t i;

    if (!disk) {
        return;
    }

    ptrcheck_usage_print();

    fat_write(disk);

    for (i = 0; i < MAX_PARTITON; i++) {
        myfree(disk->parts[i]);
    }

    sector_cache_destroy(disk);
    myfree(disk->sector0);
    myfree(disk->mbr);
    myfree(disk->fat);
    myfree(disk);
}
/*
 * disk_command_summary
 *
 * Dump summary info on partitions, like fdisk.
 */
boolean
disk_command_summary (disk_t *disk, const char *filename,
                      boolean partition_set,
                      uint32_t partition)
{
    const part_t empty_partition = {0};
    uint64_t size[MAX_PARTITON] = {0};
    uint32_t sectors_total = 0;
    uint64_t total_size = 0;
    uint32_t i;

    printf("Disk: %s, ", disk->filename);
    printf("%" PRIu32 " heads, ",
            disk->mbr->nheads);
    printf("%" PRIu32 " sec/track, ",
            disk->mbr->sectors_per_track);
    printf("%" PRIu32 " sec/cluster",
            disk->mbr->sectors_per_cluster);

    for (i = 0; i < MAX_PARTITON; i++) {
        size[i] = 
            (uint64_t) disk->parts[i]->sectors_in_partition * 
            (uint64_t) sector_size(disk);

        sectors_total += disk->parts[i]->sectors_in_partition;
        total_size += size[i];
    }

    printf("\n");
    printf("Device      Boot CylS,E HeadS,End  SecS,E     LBA  End     System\n");

    for (i = 0; i < MAX_PARTITON; i++) {
        int64_t offset = (typeof(offset)) (PART_BASE + (sizeof(part_t) * i));

        /*
         * Only interested in a partition?
         */
        if (partition_set) {
            if (partition != i) {
                continue;
            }
        }

        /*
         * Skip empty partitions.
         */
        if (!disk->parts[i]) {
            continue;
        }

        if (!memcmp(&empty_partition, disk->parts[i], sizeof(part_t))) {
            continue;
        }

        /*
         * Read the disk name.
         */
        char volname[12];

        if (fat_type(disk) == 32) {
            strncpy(volname,
                    (char*)disk->mbr->fat.fat32.volume_label,
                    sizeof(disk->mbr->fat.fat32.volume_label));
        } else if ((fat_type(disk) == 16) || (fat_type(disk) == 12)) {
            strncpy(volname,
                    (char*)disk->mbr->fat.fat16.volume_label,
                    sizeof(disk->mbr->fat.fat16.volume_label));
        } else {
            *volname = '\0';
        }

        /*
         * Don't leave any nul termination in the name we read from disk.
         */
        uint32_t j;

        for (j = 0; j < sizeof(volname); j++) {
            if (volname[j] == '\0') {
                volname[j] = ' ';
            }
        }

        volname[sizeof(volname) - 1] = '\0';

        printf("%10s %c %4u%4u  %4u%4u %4u%4u %7u %7u %-10s(%3u) ",
               volname,
               disk->parts[i]->bootable ? '*' : ' ',
               CYLINDER(disk->parts[i]->sector_start,
                        disk->parts[i]->cyl_start),
               CYLINDER(disk->parts[i]->sector_end,
                        disk->parts[i]->cyl_end),
               disk->parts[i]->head_start,
               disk->parts[i]->head_end,
               SECTOR(disk->parts[i]->sector_start),
               SECTOR(disk->parts[i]->sector_end),
               disk->parts[i]->LBA,
               disk->parts[i]->LBA +
               disk->parts[i]->sectors_in_partition - 1,
               msdos_get_systype(disk->parts[i]->os_id),
               disk->parts[i]->os_id);

        uint64_t size =
            disk->parts[i]->sectors_in_partition *
            (uint64_t) sector_size(disk);

        if (size > ONE_GIG) {
            printf("%2.2fGB", (float)size / (float)ONE_GIG);
        } else if (size > (ONE_MEG)) {
            printf("%" PRIu64 "MB", size / ONE_MEG);
        } else {
            printf("%" PRIu64 "KB", size / ONE_K);
        }

        printf("\n");
    }

    printf("\nTotal size ");

    if (total_size > ONE_GIG) {
        printf("%2.2fGB", (float)total_size / (float)ONE_GIG);
    } else if (total_size > ONE_MEG) {
        printf("%" PRIu64 "MB", total_size / ONE_MEG);
    } else {
        printf("%" PRIu64 "KB", total_size / ONE_K);
    }

    printf(", %" PRIu64 " bytes", total_size);

    printf(", total sectors %" PRIu32, sectors_total);

    printf("\n");

    return (true);
}

/*
 * sector_to_chs
 *
 * Convert sector to CHS. I'm not sure on these values as we're not talking
 * to a real disk, just a file.
 */
static void
sector_to_chs (uint32_t sector, uint32_t *c, uint32_t *h, uint32_t *s)
{
    /*
     * 6 bits for Sector; Max. 63
     */
    *s = sector & 0x3f;

    /*
     * 8 bits for Head; Max. 255
     */
    *h = (sector & 0xff00) >> 8;

    /*
     * 10 bits for Cyclinder; Max. 1023
     */
    *c = (sector & 0x3ff0000) >> 16;
}

/*
 * disk_command_format
 *
 * Format a disk for FAT12/16/32
 */
disk_t *
disk_command_format (const char *filename,
                     uint32_t partition,
                     int64_t opt_disk_start_offset,
                     boolean opt_disk_start_offset_set,
                     uint64_t format_size,
                     const char *name,
                     uint32_t sector_start,
                     uint32_t sector_end,
                     uint8_t os_id,
                     boolean zero_sectors,
                     const char *mbr,
                     uint64_t mbr_size)
{
    uint8_t dummy_mbr[512] = {
        /* 00000000 */  0xeb, 0x58, 0x90, 'f' , 'a' , 't' , 'd' , 'i' ,
                        's' , 'k' , 0x00, 0x00, 0x02, 0x08, 0x20, 0x00, /* |.X.fatdisk.... .| */
        /* 00000010 */  0x02, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00,
                        0x3f, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, /* |........?.......| */
        /* 00000020 */  0xc1, 0xff, 0x3f, 0x00, 0xf8, 0x0f, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, /* |..?.............| */
        /* 00000030 */  0x01, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 00000040 */  0x00, 0x00, 0x29, 0xd9, 0x22, 0xe8, 0xb5, 0x20,
                        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, /* |..)."..         | */
        /* 00000050 */  0x20, 0x20, 0x46, 0x41, 0x54, 0x33, 0x32, 0x20,
                        0x20, 0x20, 0x0e, 0x1f, 0xbe, 0x77, 0x7c, 0xac, /* |  FAT32   ...w|.| */
        /* 00000060 */  0x22, 0xc0, 0x74, 0x0b, 0x56, 0xb4, 0x0e, 0xbb,
                        0x07, 0x00, 0xcd, 0x10, 0x5e, 0xeb, 0xf0, 0x32, /* |".t.V.......^..2| */
        /* 00000070 */  0xe4, 0xcd, 0x16, 0xcd, 0x19, 0xeb, 0xfe, 0x54,
                        0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x6e, /* |.......This is n| */
        /* 00000080 */  0x6f, 0x74, 0x20, 0x61, 0x20, 0x62, 0x6f, 0x6f,
                        0x74, 0x61, 0x62, 0x6c, 0x65, 0x20, 0x64, 0x69, /* |ot a bootable di| */
        /* 00000090 */  0x73, 0x6b, 0x2e, 0x20, 0x20, 0x50, 0x6c, 0x65,
                        0x61, 0x73, 0x65, 0x20, 0x69, 0x6e, 0x73, 0x65, /* |sk.  Please inse| */
        /* 000000a0 */  0x72, 0x74, 0x20, 0x61, 0x20, 0x62, 0x6f, 0x6f,
                        0x74, 0x61, 0x62, 0x6c, 0x65, 0x20, 0x66, 0x6c, /* |rt a bootable fl| */
        /* 000000b0 */  0x6f, 0x70, 0x70, 0x79, 0x20, 0x61, 0x6e, 0x64,
                        0x0d, 0x0a, 0x70, 0x72, 0x65, 0x73, 0x73, 0x20, /* |oppy and..press | */
        /* 000000c0 */  0x61, 0x6e, 0x79, 0x20, 0x6b, 0x65, 0x79, 0x20,
                        0x74, 0x6f, 0x20, 0x74, 0x72, 0x79, 0x20, 0x61, /* |any key to try a| */
        /* 000000d0 */  0x67, 0x61, 0x69, 0x6e, 0x20, 0x2e, 0x2e, 0x2e,
                        0x20, 0x0d, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, /* |gain ... .......| */
        /* 000000e0 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 000000f0 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 00000100 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 00000110 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 00000120 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 00000130 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 00000140 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 00000150 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 00000160 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 00000170 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 00000180 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 00000190 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 000001a0 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 000001b0 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 000001c0 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 000001d0 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 000001e0 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* |................| */
        /* 000001f0 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xAA, /* |................| */
    };

    int64_t mbr_data_len;
    uint8_t *mbr_data;
    disk_t *disk;

    /*
     * There is a bug in KVM it seems where it underestimates the size of
     * the disk by 511 sectors.
     */
    if (sector_end - sector_start > 1024) {
        sector_end -= 1024;
    }

    mbr_data = 0;
    mbr_data_len = 0;

    /*
     * Make a dummy disk structure.
     */
    disk = (typeof(disk)) myzalloc(sizeof(*disk), __FUNCTION__);
    disk->filename = filename;
    disk->partition_set = true;
    disk->partition = partition;

    /*
     * Want to pad the disk with an empty MBR?
     */
    if (!disk->partition) {
        if (opt_disk_start_offset_set && !mbr) {

            mbr_size = opt_disk_start_offset;
            mbr_data_len = min(mbr_size, sizeof(dummy_mbr));
            mbr_data = myzalloc(mbr_data_len, __FUNCTION__);

            memcpy(mbr_data, dummy_mbr, mbr_data_len);
        }
    }

    /*
     * We have the boot MBR and then real partition data starts which has
     * the real MBR and FAT.
     */
    disk->offset = mbr_size + sector_start * opt_sector_size;

    /*
     * Map sector start and end to chs.
     */
    uint32_t c, h, s;

    /*
     * Write empty sectors to zap this partition.
     */
    uint32_t sector_chunk = 1024;

    uint8_t *empty_sector = (typeof(empty_sector))
                myzalloc(opt_sector_size * sector_chunk, __FUNCTION__);

    uint32_t sector;
    uint64_t size = (sector_end - sector_start + 1) *
                    (uint64_t) opt_sector_size;

    if (!opt_quiet) {
        OUT("Partiton %u:", partition);
    }

    if (!opt_quiet) {
        if (zero_sectors) {
            printf("  Zeroing %" PRIu32 " sectors (offset 0x%" PRIx64 
                ", sector %" PRIu64 "), ",
                sector_end - sector_start + 1,
                disk->offset,
                disk->offset / opt_sector_size);

            printf("%" PRIu64 " bytes %2.2f %2.2fM\n",
                size,
                (float)size / (float)ONE_GIG,
                (float)size / (float)ONE_MEG);
        } else {
            printf("  Zeroing only initial sectors...");
        }
    }

    boolean zeroed_end_of_disk = false;

    for (sector = sector_start; sector <= sector_end; sector += sector_chunk) {

        if (sector + sector_chunk > sector_end) {
            sector_chunk = sector_end - sector;
        }

        if (!sector_chunk) {
            break;
        }
 
        /*
         * The offset is already added in, inside sector_write_no_cache, so
         * subtract sector_start.
         */
        sector_write_no_cache(disk, sector - sector_start,
                              empty_sector, sector_chunk);

        if (sector && !(sector % (sector_chunk * 20))) {
            /*
             * Make sure we zero the end sector so the disk is full sized.
             */
            if (!zero_sectors) {
                if (!zeroed_end_of_disk) {
                    zeroed_end_of_disk = true;
                    sector = sector_end - sector_chunk - 1;
                    continue;
                }

                break;
            }

            if (!opt_quiet) {
                printf(".");
                fflush(stdout);
            }
        }

    }

    myfree(empty_sector);

    if (!opt_quiet) {
        printf("\n");
    }

    /*
     * Read all partitions and then set ours up.
     */
    partition_table_read(disk);

    part_t *part = disk->parts[partition];

    memset(part, 0, sizeof(*part));

    /*
     * If a MBR is given, fair bet it is bootable.
     */
    if (mbr) {
        part->bootable = 0x80;
    }

    part->os_id = os_id;

    /*
     * CHS addressing begins from 0/0/1.
     */
    sector_to_chs(sector_start + 1, &c, &h, &s);
    part->cyl_start = c & 0xff;
    part->head_start = h;
    part->sector_start = (s & 0x3f) | ((c & 0x300) >> 2);

    sector_to_chs(sector_end, &c, &h, &s);
    part->cyl_end = c & 0xff;
    part->head_end = h;
    part->sector_end = (s & 0x3f) | ((c & 0x300) >> 2);

    /*
     * Except the values above seem meaningless. Choose some examples that
     * came from fdisk for a 2G disk that seem to work.
     */
#if 0
    part->cyl_start = 0;
    part->head_start = 1;
    part->sector_start = 1;
    part->cyl_end = 248;
    part->head_end = 7;
    part->sector_end = 28;
#endif

    part->LBA = sector_start + (uint32_t) (mbr_size / opt_sector_size);
    part->sectors_in_partition = sector_end - sector_start + 1 -
            (mbr_size / opt_sector_size);

    /*
     * Allocate space for the whole boot program.
     */
    if (mbr) {
        if (!mbr_data) {
            mbr_data = (typeof(mbr_data))
                            myzalloc((uint32_t) mbr_size, __FUNCTION__);

            uint8_t *data;

            data = file_read(mbr, &mbr_data_len);
            if (!data) {
                ERR("Failed to read %s for placing on disk image", mbr);
            }

            if (mbr_data_len > (int64_t) mbr_size) {
                DIE("expected MBR len %" PRIu64 " < file size %" PRId64,
                    mbr_size, mbr_data_len);
            }

            memcpy(mbr_data, data, mbr_data_len);

            myfree(data);
        }
    }

    if (mbr_data) {
        if (opt_quiet) {
            /*
             * Silence.
             */
        } else if (opt_verbose) {
            OUT("  Writing bootloader to partition %" PRIu32 ", size %" PRIu64 
                " bytes, %" PRIu64 " sectors "
                "(first %" PRIu32 " bytes follows):",
                partition, mbr_size, mbr_size / opt_sector_size,
                opt_sector_size);

            hex_dump(mbr_data, 0, opt_sector_size);
        } else {
            OUT("  Writing bootloader to partition %" PRIu32 ", size %" PRIu64 
                " bytes, %" PRIu64 " sectors",
                partition, mbr_size, mbr_size / opt_sector_size);
        }

        file_write_at(disk->filename, sector_start * opt_sector_size,
                      mbr_data, mbr_data_len);

        myfree(mbr_data);
    }

    /*
     * Now read the real MBR at the start of the partition. This should all be
     * 0.
     */
    disk->mbr = (typeof(disk->mbr))
                    disk_read_from(disk, 0, opt_sector_size * 2);
    if (!disk->mbr) {
        ERR("No boot record read");
        myfree(disk);
        return (0);
    }

    /*
     * Write a fake MBR with jump codes so that we see something on boot
     * if booting this MBR is attempted.
     */
    memcpy(disk->mbr, dummy_mbr, sizeof(dummy_mbr));

    disk->mbr->sector_size = opt_sector_size;

    switch (os_id) {
        case DISK_FAT12:
            disk->mbr->sectors_per_cluster = 1;
            disk->mbr->number_of_dirents = 512;
            break;

        case DISK_FAT16:
        case DISK_FAT16_LBA:
            if (size >= (uint64_t) ONE_GIG) {
                disk->mbr->sectors_per_cluster = 64;
            } else if (size >= 512 * ONE_MEG) {
                disk->mbr->sectors_per_cluster = 32;
            } else if (size >= 256 * ONE_MEG) {
                disk->mbr->sectors_per_cluster = 16;
            } else if (size >= 128 * ONE_MEG) {
                disk->mbr->sectors_per_cluster = 8;
            } else {
                disk->mbr->sectors_per_cluster = 4;
            }

            disk->mbr->number_of_dirents = 512;
            break;

        case DISK_FAT32:
        case DISK_FAT32_LBA:
            if (size >= 32LLU * (uint64_t) ONE_GIG) {
                disk->mbr->sectors_per_cluster = 64;
            } else if (size >= 16LLU * (uint64_t) ONE_GIG) {
                disk->mbr->sectors_per_cluster = 32;
            } else if (size >= 8LLU * (uint64_t) ONE_GIG) {
                disk->mbr->sectors_per_cluster = 16;
            } else {
                disk->mbr->sectors_per_cluster = 8;
            }

            disk->mbr->number_of_dirents = 0;
            break;
    }

    if (opt_sectors_per_cluster) {
        disk->mbr->sectors_per_cluster = opt_sectors_per_cluster;
    }

    disk->mbr->sector_size = opt_sector_size;
    disk->mbr->reserved_sector_count = 32;
    disk->mbr->number_of_fats = 2;

    uint32_t sector_count = sector_end - sector_start + 1;
    if (sector_count > 0xffff) {
        disk->mbr->sector_count = 0;
    }

    disk->mbr->sector_count_large = sector_count -
        (mbr_size / opt_sector_size);
    disk->mbr->media_type = 0xF8;
    disk->mbr->sectors_hidden = 0;
    disk->mbr->sectors_per_track = 63;
    disk->mbr->nheads = 255;

    uint64_t fat_bytes;

    /*
     * Make the maximum sized FAT possible. The actual disk is limited in how
     * it can address clusters by subtracting the FAT size from the disk size,
     * so we should not ever overrun the disk.
     */
    switch (os_id) {
        case DISK_FAT12:
            fat_bytes =
                (3 * (sector_count / disk->mbr->sectors_per_cluster)) / 2;
            break;
        case DISK_FAT16:
        case DISK_FAT16_LBA:
            fat_bytes = 2 * (sector_count / disk->mbr->sectors_per_cluster);
            break;
        case DISK_FAT32:
        case DISK_FAT32_LBA:
            fat_bytes = 4 * (sector_count / disk->mbr->sectors_per_cluster);
            break;
        default:
            fat_bytes = 0;
            break;
    }

    uint32_t fat_size_sectors = fat_bytes / opt_sector_size;

    switch (os_id) {
        case DISK_FAT12:
        case DISK_FAT16:
        case DISK_FAT16_LBA:
        case DISK_FAT32:
        case DISK_FAT32_LBA:
            if (!fat_size_sectors) {
                DIE("disk is too small any FAT");
            }
            break;
    }

    /*
     * Read the disk name.
     */
    const char default_volname[] = "fatdisk       ";

    char volname[sizeof(disk->mbr->fat.fat16.volume_label) + 1];

    if (name) {
        strncpy((char *)volname, name, sizeof(volname));
    } else{
        strncpy((char *)volname, default_volname, sizeof(volname));
    }

    /*
     * Don't leave any nul termination in the name we read from disk.
     */
    uint32_t i;

    for (i = 0; i < sizeof(volname); i++) {
        if (volname[i] == '\0') {
            volname[i] = ' ';
        }
    }

    volname[sizeof(volname) - 1] = '\0';

    switch (os_id) {
        case DISK_FAT12:
            disk->mbr->fat_size_sectors = fat_size_sectors;
            disk->mbr->fat.fat16.bios_drive_num = 0x80;
            disk->mbr->fat.fat16.reserved1 = 0;
            disk->mbr->fat.fat16.boot_signature = 0;
            disk->mbr->fat.fat16.volume_id = 0;
            strncpy((char *)disk->mbr->fat.fat16.volume_label, volname,
                    sizeof(disk->mbr->fat.fat16.volume_label));
            strncpy((char *)disk->mbr->fat.fat16.fat_type_label, "FAT12",
                    sizeof(disk->mbr->fat.fat16.fat_type_label));
            break;

        case DISK_FAT16:
        case DISK_FAT16_LBA:
            disk->mbr->fat_size_sectors = fat_size_sectors;
            disk->mbr->fat.fat16.bios_drive_num = 0x80;
            disk->mbr->fat.fat16.reserved1 = 0;
            disk->mbr->fat.fat16.boot_signature = 0;
            disk->mbr->fat.fat16.volume_id = 0;
            strncpy((char *)disk->mbr->fat.fat16.volume_label, volname,
                    sizeof(disk->mbr->fat.fat16.volume_label));
            strncpy((char *)disk->mbr->fat.fat16.fat_type_label, "FAT16",
                    sizeof(disk->mbr->fat.fat16.fat_type_label));
            break;

        case DISK_FAT32:
        case DISK_FAT32_LBA:
            disk->mbr->fat.fat32.fat_size_sectors = fat_size_sectors;
            disk->mbr->fat.fat32.extended_flags = 0;
            disk->mbr->fat.fat32.fat_version = 0;
            disk->mbr->fat.fat32.root_cluster = 2;
            disk->mbr->fat.fat32.fat_info = 1;
            disk->mbr->fat.fat32.backup_boot_sector = 0;
            disk->mbr->fat.fat32.drive_number = 0;
            disk->mbr->fat.fat32.boot_signature = 0x29;
            disk->mbr->fat.fat32.volume_id = 0xFE291AF7;
            strncpy((char *)disk->mbr->fat.fat32.volume_label, volname,
                    sizeof(disk->mbr->fat.fat32.volume_label));
            strncpy((char *)disk->mbr->fat.fat32.fat_type_label, "FAT32",
                    sizeof(disk->mbr->fat.fat32.fat_type_label));
            break;
    }

    switch (os_id) {
        case DISK_FAT12:
        case DISK_FAT16:
        case DISK_FAT16_LBA:
        case DISK_FAT32:
        case DISK_FAT32_LBA:
            if (!fat_format(disk, partition, os_id)) {
                DIE("failed to format disk");
            }

            break;
    }

    disk_write_at(disk, 0, (uint8_t *) disk->mbr, opt_sector_size * 2);

    /*
     * Write partitions back to disk, over the MBR above.
     */
    if (!partition_table_write(disk)) {
        ERR("Failed in writing partition %" PRIu32 "", partition);
    }

    return (disk);
}

/*
 * disk_command_list
 *
 * List files on disk
 */
uint32_t
disk_command_list (disk_t *disk, const char *filter)
{
    const char *dir_name = "";
    uint32_t count;
    disk_walk_args_t args = {0};

    args.print = true;

    count = disk_walk(disk, filter, dir_name, 0, 0, 0, &args);

    return (count);
}

/*
 * disk_command_find
 *
 * find files on disk
 */
uint32_t
disk_command_find (disk_t *disk, const char *filter)
{
    const char *dir_name = "";
    uint32_t count;
    disk_walk_args_t args = {0};

    args.find = true;
    args.print = true;
    args.walk_whole_tree = true;

    count = disk_walk(disk, filter, dir_name, 0, 0, 0, &args);

    return (count);
}

/*
 * disk_command_hex_dump
 *
 * Hexdump files on disk
 */
uint32_t
disk_command_hex_dump (disk_t *disk, const char *filter)
{
    const char *dir_name = "";
    uint32_t count;
    disk_walk_args_t args = {0};

    args.hexdump = true;

    count = disk_walk(disk, filter, dir_name, 0, 0, 0, &args);

    return (count);
}

/*
 * disk_command_cat
 *
 * cat files on disk
 */
uint32_t
disk_command_cat (disk_t *disk, const char *filter)
{
    const char *dir_name = "";
    uint32_t count;
    disk_walk_args_t args = {0};

    args.cat = true;

    count = disk_walk(disk, filter, dir_name, 0, 0, 0, &args);

    return (count);
}

/*
 * disk_command_extract
 *
 * Extract files on disk
 */
uint32_t
disk_command_extract (disk_t *disk, const char *filter)
{
    disk_walk_args_t args = {0};
    const char *dir_name = "";
    uint32_t count;

    args.extract = true;
    count = disk_walk(disk, filter, dir_name, 0, 0, 0, &args);

    return (count);
}

/*
 * disk_command_remove
 *
 * Delete files on disk
 */
uint32_t
disk_command_remove (disk_t *disk, const char *filter)
{
    const char *dir_name = "";
    uint32_t count;
    disk_walk_args_t args = {0};

    args.remove = true;
    count = disk_walk(disk, filter, dir_name, 0, 0, 0, &args);

    return (count);
}

/*
 * disk_add
 *
 * Add files to the disk
 */
uint32_t disk_add (disk_t *disk,
                   const char *source_file_or_dir,
                   const char *target_file_or_dir)
{
    tree_file_node *n;
    tree_root *d;
    uint32_t count;

    if (!source_file_or_dir) {
        source_file_or_dir = ".";
    }

    if (!target_file_or_dir) {
        target_file_or_dir = ".";
    }

    /*
     * If a dir, import all files in the dir.
     */
    count = 0;

    if (dir_exists(source_file_or_dir)) {
        count = disk_command_add_file_or_dir(disk,
                                             source_file_or_dir,
                                             source_file_or_dir,
                                             false /* addfile */);

        d = dirlist_recurse(source_file_or_dir, 0, 0, true /* include dirs */);
        if (!d) {
            DIE("Cannot list dir %s", source_file_or_dir);
        }

        TREE_WALK(d, n) {
            char *file = strsub(n->tree.key, "./", "");

            if (strcmp(file, ".") && strcmp(file, "..")) {
                count += disk_command_add_file_or_dir(disk, file, file,
                                                      false /* addfile */);
            }

            myfree(file);
        }

        tree_empty(d, 0);

        myfree(d);
    } else {
        count = disk_command_add_file_or_dir(disk,
                                             source_file_or_dir,
                                             target_file_or_dir,
                                             false /* addfile */);
    }

    return (count);
}

/*
 * disk_addfile
 *
 * Add files to the disk, but this time the target name is different from the 
 * source.
 */
uint32_t disk_addfile (disk_t *disk,
                       const char *source_file_or_dir,
                       const char *target_file_or_dir)
{
    tree_file_node *n;
    tree_root *d;
    uint32_t count;

    if (!source_file_or_dir) {
        source_file_or_dir = ".";
    }

    if (!target_file_or_dir) {
        target_file_or_dir = ".";
    }

    /*
     * If a dir, import all files in the dir.
     */
    count = 0;

    if (dir_exists(source_file_or_dir)) {
        count = disk_command_add_file_or_dir(disk,
                                             source_file_or_dir,
                                             source_file_or_dir,
                                             true /* addfile */);

        d = dirlist_recurse(source_file_or_dir, 0, 0, true /* include dirs */);
        if (!d) {
            DIE("Cannot list dir %s", source_file_or_dir);
        }

        TREE_WALK(d, n) {
            char *file = strsub(n->tree.key, "./", "");

            if (strcmp(file, ".") && strcmp(file, "..")) {
                count += disk_command_add_file_or_dir(disk, file, file,
                                                      true /* addfile */);
            }

            myfree(file);
        }

        tree_empty(d, 0);

        myfree(d);
    } else {
        count = disk_command_add_file_or_dir(disk,
                                             source_file_or_dir,
                                             target_file_or_dir,
                                             true /* addfile */);
    }

    return (count);
}

/*
 * disk_command_query_at_offset
 *
 * Look for a viable MSDOS boot sector at this offset.
 */
static uint32_t
disk_command_query_at_offset (const char *filename, int64_t offset)
{
    uint32_t fat;
    disk_t *disk;

    disk = (typeof(disk)) myzalloc(opt_sector_size, __FUNCTION__);
    if (!disk) {
        return (0);
    }

    disk->filename = filename;
    disk->offset = offset;
    disk->mbr = (typeof(disk->mbr)) disk_read_from(disk, 0, opt_sector_size);
    if (!disk->mbr) {
        myfree(disk);
        return (0);
    }

    if (!sector_size(disk)) {
        myfree(disk);
        return (0);
    }

    disk->sector0 = (typeof(disk->sector0)) disk->mbr;
    if (!disk->sector0) {
        myfree(disk->mbr);
        myfree(disk);
        return (0);
    }

    if ((disk->sector0[opt_sector_size - 2] != 0x55) ||
        (disk->sector0[opt_sector_size - 1] != 0xAA)) {
        myfree(disk->mbr);
        myfree(disk);
        return (0);
    }

    if (!disk->mbr->sectors_per_cluster) {
        myfree(disk->mbr);
        myfree(disk);
        return (0);
    }

    if (disk->mbr->sector_size < opt_sector_size) {
        myfree(disk->mbr);
        myfree(disk);
        return (0);
    }

    if (disk->mbr->sector_size % opt_sector_size) {
        myfree(disk->mbr);
        myfree(disk);
        return (0);
    }

    if (!disk->mbr->number_of_fats) {
        myfree(disk->mbr);
        myfree(disk);
        return (0);
    }

    if (disk->mbr->number_of_fats > 2) {
        myfree(disk->mbr);
        myfree(disk);
        return (0);
    }

    fat = fat_type(disk);

    switch (fat) {
    case 32:
    case 16:
    case 12:
        myfree(disk->mbr);
        myfree(disk);
        return (fat);
    }

    myfree(disk->mbr);
    myfree(disk);

    return (0);
}

/*
 * disk_command_query_hunt
 *
 * Try and find the DOS header.
 */
static uint64_t
disk_command_query_hunt (const char *filename, uint32_t *fat_type)
{
    uint64_t sz = file_size(filename);
    uint64_t first_one_found;
    uint32_t found_fat_type;
    uint64_t offset;

    first_one_found = 0;
    *fat_type = 0;

    for (offset = 0x0; offset < min(sz, 0xffffff); offset += 0x100) {

        found_fat_type = disk_command_query_at_offset(filename, offset);
        if (found_fat_type) {
            *fat_type = found_fat_type;

            VER("FAT %" PRIu32 " filesystem found at offset 0x%llx",
                found_fat_type,
                (unsigned long long)offset);

            if (!first_one_found) {
                first_one_found = offset;
                break;
            }
        }
    }

    if (first_one_found) {
        VER("Using FAT filesystem found at offset 0x%llx",
            (unsigned long long)first_one_found);
    }

    return (first_one_found);
}

/*
 * disk_command_query
 *
 * Try and find the DOS header.
 */
uint64_t
disk_command_query (const char *filename,
                    uint32_t partition,
                    boolean partition_set,
                    boolean hunt)
{
    const part_t empty_partition = {0};
    uint32_t fat_type;
    uint64_t offset;
    uint32_t amount;
    uint32_t i;
    part_t *p;

    uint32_t start;
    uint32_t end;

    start = 0;
    end = MAX_PARTITON;

    if (partition_set) {
        start = partition;
        end = partition + 1;
    }

    /*
     * Use the partition tables first.
     */
    for (i = start; i < end; i++) {
        offset = (typeof(offset)) (PART_BASE + (sizeof(part_t) * i));
        amount = sizeof(part_t);

        p = (part_t *) file_read_from(filename, offset, amount);
        if (!p) {
            continue;
        }

        if (!memcmp(&empty_partition, p, sizeof(part_t))) {
            continue;
        }

        offset = opt_sector_size * p->LBA;
        fat_type = disk_command_query_at_offset(filename, offset);

        if (fat_type) {
            VER("Using DOSFS in partition %" PRIu32 ", sector %" PRIu32
                " offset 0x%" PRIx64, i, p->LBA, offset);

            myfree(p);
            return (offset);
        }

        myfree(p);
    }

    if (!hunt) {
        return (0);
    }

    /*
     * Now brute force.
     */
    VER("No DOSFS found from partition table, try brute force search...");

    offset = disk_command_query_hunt(filename, &fat_type);
    if (!fat_type) {
        ERR("No DOSFS found via the partition table or by searching. "
            "Is '%s' a DOS disk?", filename);
    }

    return (offset);
}

