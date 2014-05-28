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

/*
 * msdos_get_systype
 *
 * Convert disk type to string.
 */
const char *
msdos_get_systype (uint32_t index)
{
    switch (index) {
    case 0x00: return ("Empty");
    case 0x01: return ("FAT12");
    case 0x02: return ("XENIX-root");
    case 0x03: return ("XENIX-usr");
    case 0x04: return ("Small-FAT16");
    case 0x05: return ("Extended");
    case 0x06: return ("FAT16");
    case 0x07: return ("HPFS/NTFS");
    case 0x08: return ("AIX");
    case 0x09: return ("AIX-bootable");
    case 0x0a: return ("OS/2-boot-mgr");
    case 0x0b: return ("FAT32");
    case 0x0c: return ("FAT32-LBA");
    case 0x0e: return ("FAT16-LBA");
    case 0x0f: return ("Extended-LBA");
    case 0x10: return ("OPUS");
    case 0x11: return ("Hidden-FAT12");
    case 0x12: return ("Compaq-diag");
    case 0x14: return ("Hidd-Sm-FAT16");
    case 0x16: return ("Hidd-FAT16");
    case 0x17: return ("Hidd-HPFS/NTFS");
    case 0x18: return ("AST-SmartSleep");
    case 0x1b: return ("Hidd-FAT32");
    case 0x1c: return ("Hidd-FAT32-LBA");
    case 0x1e: return ("Hidd-FAT16-LBA");
    case 0x24: return ("NEC-DOS");
    case 0x39: return ("Plan-9");
    case 0x3c: return ("PMagic-recovery");
    case 0x40: return ("Venix-80286");
    case 0x41: return ("PPC-PReP-Boot");
    case 0x42: return ("SFS");
    case 0x4d: return ("QNX4.x");
    case 0x4e: return ("QNX4.x-2nd-part");
    case 0x4f: return ("QNX4.x-3rd-part");
    case 0x50: return ("OnTrack-DM");
    case 0x51: return ("OnTrackDM6-Aux1");
    case 0x52: return ("CP/M");
    case 0x53: return ("OnTrackDM6-Aux3");
    case 0x54: return ("OnTrack-DM6");
    case 0x55: return ("EZ-Drive");
    case 0x56: return ("Golden-Bow");
    case 0x5c: return ("Priam-Edisk");
    case 0x61: return ("SpeedStor");
    case 0x63: return ("GNU-HURD/SysV");
    case 0x64: return ("Netware-286");
    case 0x65: return ("Netware-386");
    case 0x70: return ("DiskSec-MltBoot");
    case 0x75: return ("PC/IX");
    case 0x80: return ("Minix-<1.4a");
    case 0x81: return ("Minix->1.4b");
    case 0x82: return ("Linux-swap");
    case 0x83: return ("Linux");
    case 0x84: return ("OS/2-hidden-C:");
    case 0x85: return ("Linux-extended");
    case 0x86: return ("NTFS-volume-set");
    case 0x87: return ("NTFS-volume-set");
    case 0x88: return ("Linux-plaintext");
    case 0x8e: return ("Linux-LVM");
    case 0x93: return ("Amoeba");
    case 0x94: return ("Amoeba-BBT");
    case 0x9f: return ("BSD/OS");
    case 0xa0: return ("Thinkpad-hib");
    case 0xa5: return ("FreeBSD");
    case 0xa6: return ("OpenBSD");
    case 0xa7: return ("NeXTSTEP");
    case 0xa8: return ("Darwin-UFS");
    case 0xa9: return ("NetBSD");
    case 0xab: return ("Darwin-boot");
    case 0xb7: return ("BSDI-fs");
    case 0xb8: return ("BSDI-swap");
    case 0xbb: return ("Boot-Wizard-Hid");
    case 0xbe: return ("Solaris-boot");
    case 0xbf: return ("Solaris");
    case 0xc1: return ("DRDOS/2-FAT12");
    case 0xc4: return ("DRDOS/2-smFAT16");
    case 0xc6: return ("DRDOS/2-FAT16");
    case 0xc7: return ("Syrinx");
    case 0xda: return ("Non-FS-data");
    case 0xdb: return ("CP/M/CTOS");
    case 0xde: return ("Dell-Utility");
    case 0xdf: return ("BootIt");
    case 0xe1: return ("DOS-access");
    case 0xe3: return ("DOS-R/O");
    case 0xe4: return ("SpeedStor");
    case 0xeb: return ("BeOS-fs");
    case 0xee: return ("EFI-GPT");
    case 0xef: return ("EFI-FAT");
    case 0xf0: return ("Lnx/PA-RISC-bt");
    case 0xf1: return ("SpeedStor");
    case 0xf2: return ("DOS-secondary");
    case 0xf4: return ("SpeedStor");
    case 0xfd: return ("Lnx-RAID-auto");
    case 0xfe: return ("LANstep");
    case 0xff: return ("XENIX-BBT");
    }

    return ("");
}

/*
 * traceback_alloc
 *
 * Convert string back to dos type.
 */
uint8_t
msdos_parse_systype (char *in)
{
    if (!strcasecmp(in, "Empty")) {
        return (0x00);
    }
    if (!strcasecmp(in, "FAT12")) {
        return (0x01);
    }
    if (!strcasecmp(in, "XENIX-root")) {
        return (0x02);
    }
    if (!strcasecmp(in, "XENIX-usr")) {
        return (0x03);
    }
    if (!strcasecmp(in, "Small-FAT16")) {
        return (0x04);
    }
    if (!strcasecmp(in, "Extended")) {
        return (0x05);
    }
    if (!strcasecmp(in, "FAT16")) {
        return (0x06);
    }
    if (!strcasecmp(in, "HPFS/NTFS")) {
        return (0x07);
    }
    if (!strcasecmp(in, "AIX")) {
        return (0x08);
    }
    if (!strcasecmp(in, "AIX-bootable")) {
        return (0x09);
    }
    if (!strcasecmp(in, "OS/2-boot-mgr")) {
        return (0x0a);
    }
    if (!strcasecmp(in, "FAT32")) {
        return (0x0b);
    }
    if (!strcasecmp(in, "FAT32-LBA")) {
        return (0x0c);
    }
    if (!strcasecmp(in, "FAT16-LBA")) {
        return (0x0e);
    }
    if (!strcasecmp(in, "Extended-LBA")) {
        return (0x0f);
    }
    if (!strcasecmp(in, "OPUS")) {
        return (0x10);
    }
    if (!strcasecmp(in, "Hidden-FAT12")) {
        return (0x11);
    }
    if (!strcasecmp(in, "Compaq-diag")) {
        return (0x12);
    }
    if (!strcasecmp(in, "Hidd Sm-FAT16")) {
        return (0x14);
    }
    if (!strcasecmp(in, "Hidd-FAT16")) {
        return (0x16);
    }
    if (!strcasecmp(in, "Hidd-HPFS/NTFS")) {
        return (0x17);
    }
    if (!strcasecmp(in, "AST-SmartSleep")) {
        return (0x18);
    }
    if (!strcasecmp(in, "Hidd-FAT32")) {
        return (0x1b);
    }
    if (!strcasecmp(in, "Hidd-FAT32-LBA")) {
        return (0x1c);
    }
    if (!strcasecmp(in, "Hidd-FAT16-LBA")) {
        return (0x1e);
    }
    if (!strcasecmp(in, "NEC-DOS")) {
        return (0x24);
    }
    if (!strcasecmp(in, "Plan-9")) {
        return (0x39);
    }
    if (!strcasecmp(in, "PMagic-recovery")) {
        return (0x3c);
    }
    if (!strcasecmp(in, "Venix-80286")) {
        return (0x40);
    }
    if (!strcasecmp(in, "PPC-PReP-Boot")) {
        return (0x41);
    }
    if (!strcasecmp(in, "SFS")) {
        return (0x42);
    }
    if (!strcasecmp(in, "QNX4.x")) {
        return (0x4d);
    }
    if (!strcasecmp(in, "QNX4.x-2nd-part")) {
        return (0x4e);
    }
    if (!strcasecmp(in, "QNX4.x-3rd-part")) {
        return (0x4f);
    }
    if (!strcasecmp(in, "OnTrack-DM")) {
        return (0x50);
    }
    if (!strcasecmp(in, "OnTrackDM6-Aux1")) {
        return (0x51);
    }
    if (!strcasecmp(in, "CP/M")) {
        return (0x52);
    }
    if (!strcasecmp(in, "OnTrackDM6-Aux3")) {
        return (0x53);
    }
    if (!strcasecmp(in, "OnTrack-DM6")) {
        return (0x54);
    }
    if (!strcasecmp(in, "EZ-Drive")) {
        return (0x55);
    }
    if (!strcasecmp(in, "Golden-Bow")) {
        return (0x56);
    }
    if (!strcasecmp(in, "Priam-Edisk")) {
        return (0x5c);
    }
    if (!strcasecmp(in, "SpeedStor")) {
        return (0x61);
    }
    if (!strcasecmp(in, "GNU-HURD/SysV")) {
        return (0x63);
    }
    if (!strcasecmp(in, "Netware-286")) {
        return (0x64);
    }
    if (!strcasecmp(in, "Netware-386")) {
        return (0x65);
    }
    if (!strcasecmp(in, "DiskSec-MltBoot")) {
        return (0x70);
    }
    if (!strcasecmp(in, "PC/IX")) {
        return (0x75);
    }
    if (!strcasecmp(in, "Minix-<1.4a")) {
        return (0x80);
    }
    if (!strcasecmp(in, "Minix->1.4b")) {
        return (0x81);
    }
    if (!strcasecmp(in, "Linux-swap")) {
        return (0x82);
    }
    if (!strcasecmp(in, "Linux")) {
        return (0x83);
    }
    if (!strcasecmp(in, "OS/2-hidden-C:")) {
        return (0x84);
    }
    if (!strcasecmp(in, "Linux-extended")) {
        return (0x85);
    }
    if (!strcasecmp(in, "NTFS-volume-set")) {
        return (0x86);
    }
    if (!strcasecmp(in, "NTFS-volume-set")) {
        return (0x87);
    }
    if (!strcasecmp(in, "Linux-plaintext")) {
        return (0x88);
    }
    if (!strcasecmp(in, "Linux-LVM")) {
        return (0x8e);
    }
    if (!strcasecmp(in, "Amoeba")) {
        return (0x93);
    }
    if (!strcasecmp(in, "Amoeba-BBT")) {
        return (0x94);
    }
    if (!strcasecmp(in, "BSD/OS")) {
        return (0x9f);
    }
    if (!strcasecmp(in, "Thinkpad-hib")) {
        return (0xa0);
    }
    if (!strcasecmp(in, "FreeBSD")) {
        return (0xa5);
    }
    if (!strcasecmp(in, "OpenBSD")) {
        return (0xa6);
    }
    if (!strcasecmp(in, "NeXTSTEP")) {
        return (0xa7);
    }
    if (!strcasecmp(in, "Darwin-UFS")) {
        return (0xa8);
    }
    if (!strcasecmp(in, "NetBSD")) {
        return (0xa9);
    }
    if (!strcasecmp(in, "Darwin-boot")) {
        return (0xab);
    }
    if (!strcasecmp(in, "BSDI-fs")) {
        return (0xb7);
    }
    if (!strcasecmp(in, "BSDI-swap")) {
        return (0xb8);
    }
    if (!strcasecmp(in, "Boot-Wizard-Hid")) {
        return (0xbb);
    }
    if (!strcasecmp(in, "Solaris-boot")) {
        return (0xbe);
    }
    if (!strcasecmp(in, "Solaris")) {
        return (0xbf);
    }
    if (!strcasecmp(in, "DRDOS/2-FAT12")) {
        return (0xc1);
    }
    if (!strcasecmp(in, "DRDOS/2-smFAT16")) {
        return (0xc4);
    }
    if (!strcasecmp(in, "DRDOS/2-FAT16")) {
        return (0xc6);
    }
    if (!strcasecmp(in, "Syrinx")) {
        return (0xc7);
    }
    if (!strcasecmp(in, "Non-FS-data")) {
        return (0xda);
    }
    if (!strcasecmp(in, "CP/M-/-CTOS")) {
        return (0xdb);
    }
    if (!strcasecmp(in, "Dell-Utility")) {
        return (0xde);
    }
    if (!strcasecmp(in, "BootIt")) {
        return (0xdf);
    }
    if (!strcasecmp(in, "DOS-access")) {
        return (0xe1);
    }
    if (!strcasecmp(in, "DOS-R/O")) {
        return (0xe3);
    }
    if (!strcasecmp(in, "SpeedStor")) {
        return (0xe4);
    }
    if (!strcasecmp(in, "BeOS-fs")) {
        return (0xeb);
    }
    if (!strcasecmp(in, "EFI-GPT")) {
        return (0xee);
    }
    if (!strcasecmp(in, "EFI-FAT")) {
        return (0xef);
    }
    if (!strcasecmp(in, "Lnx/PA-RISC-bt")) {
        return (0xf0);
    }
    if (!strcasecmp(in, "SpeedStor")) {
        return (0xf1);
    }
    if (!strcasecmp(in, "DOS-secondary")) {
        return (0xf2);
    }
    if (!strcasecmp(in, "SpeedStor")) {
        return (0xf4);
    }
    if (!strcasecmp(in, "Lnx-RAID-auto")) {
        return (0xfd);
    }
    if (!strcasecmp(in, "LANstep")) {
        return (0xfe);
    }
    if (!strcasecmp(in, "XENIX-BBT")) {
        return (0xff);
    }

    return (0xff);
}

/*
 * msdos_get_media_type
 *
 * Media type to string.
 */
const char *
msdos_get_media_type (uint32_t index)
{
    switch (index) {

    case 0xE5: return

    "\n"
    "    8-inch (200 mm) Single sided, 77 tracks per side, 26 sectors\n"
    "    per track, 128 bytes per sector (243 KB) (DR-DOS only)";

    case 0xED: return

    "\n"
    "    5.25-inch (130 mm) Double sided, 80 tracks per side, 9 sector,\n"
    "    720 KB (Tandy 2000 only)";

    case 0xF0: return

    "\n"
    "    3.5-inch (90 mm) Double Sided, 80 tracks per side, 18 or 36\n"
    "    sectors per track (1.44 MB or 2.88 MB).\n"
    "    Designated for use with custom floppy and superfloppy formats\n"
    "    where the geometry is defined in the BPB.\n"
    "    Used also for other media types such as tapes.";

    case 0xF8: return

    "\n"
    "    Fixed disk (i.e., typically a partition on a hard disk).\n"
    "    (since DOS 2.0)\n"
    "    Designated to be used for any partitioned fixed or removable\n"
    "    media, where the geometry is defined in the BPB.\n"
    "    3.5-inch Single sided, 80 tracks per side, 9 sectors per track\n"
    "    (360 KB) (MSX-DOS only)\n"
    "    5.25-inch Double sided, 80 tracks per side, 9 sectors per track\n"
    "    (720 KB) (Sanyo 55x DS-DOS 2.11 only)";

    case 0xF9: return

    "\n"
    "    3.5-inch Double sided, 80 tracks per side, 9 sectors per track\n"
    "    (720 KB) (since DOS 3.2)\n"
    "    3.5-inch Double sided, 80 tracks per side, 18 sectors per track\n"
    "    (1440 KB) (DOS 3.2 only)\n"
    "    5.25-inch Double sided, 80 tracks per side, 15 sectors per track\n"
    "    (1.2 MB) (since DOS 3.0)";

    case 0xFA: return

    "\n"
    "    3.5-inch and 5.25-inch Single sided, 80 tracks per side, 8\n"
    "    sectors per track (320 KB)\n"
    "    Used also for RAM disks and ROM disks (f.e. on HP 200LX)\n"
    "    Hard disk (Tandy MS-DOS only)";

    case 0xFB: return

    "\n"
    "    3.5-inch and 5.25-inch Double sided, 80 tracks per side,\n"
    "    8 sectors per track (640 KB)";

    case 0xFC: return

    "\n"
    "    5.25-inch Single sided, 40 tracks per side, 9 sectors per track\n"
    "    (180 KB) (since DOS 2.0)";

    case 0xFD: return

    "\n"
    "    5.25-inch Double sided, 40 tracks per side, 9 sectors per track\n"
    "    (360 KB) (since DOS 2.0)\n"
    "    8-inch Double sided, 77 tracks per side, 26 sectors per track,\n"
    "    128 bytes per sector (500.5 KB)\n"
    "    (8-inch Double sided, (single and) double density (DOS 1))";

    case 0xFE: return

    "\n"
    "    5.25-inch Single sided, 40 tracks per side, 8 sectors per track\n"
    "    (160 KB) (since DOS 1.0)\n"
    "    8-inch Single sided, 77 tracks per side, 26 sectors per track,\n"
    "    128 bytes per sector (250.25 KB)\n"
    "    8-inch Double sided, 77 tracks per side, 8 sectors per track,\n"
    "    ONE_K bytes per sector (1232 KB)\n"
    "    (8-inch Single sided, (single and) double density (DOS 1))";

    case 0xFF: return

    "\n"
    "    5.25-inch Double sided, 40 tracks per side, 8 sectors per track\n"
    "    (320 KB) (since DOS 1.1)\n"
    "    Hard disk (Sanyo 55x DS-DOS 2.11 only)";
    }

    return ("");
}

/*
 * disk_read_from
 *
 * Read raw bytes from the disk at a given offset.
 */
uint8_t *
disk_read_from (disk_t *disk, uint64_t offset, uint64_t len)
{
    DBG4("Read from disk, len %" PRIu64 " bytes", len);

    return (file_read_from(disk->filename, offset + disk->offset, len));
}

/*
 * disk_write_at
 *
 * Write raw bytes to the disk.
 */
boolean
disk_write_at (disk_t *disk, uint64_t offset, uint8_t *data,
               uint64_t len)
{
    DBG4("Write to disk, len %" PRIu64 " bytes", len);

    return (file_write_at(disk->filename, offset + disk->offset,
                          data, len) == 0);
}

/*
 * disk_hex_dump
 *
 * Dump raw bytes read from the disk.
 */
boolean
disk_hex_dump (disk_t *disk, void *addr, uint64_t offset, uint64_t len)
{
    return (hex_dump(addr, offset + disk->offset, len));
}

/*
 * disk_cat
 *
 * Dump raw bytes read from the disk.
 */
boolean
disk_cat (disk_t *disk, void *addr, uint64_t offset, uint64_t len)
{
    return (cat(addr, offset + disk->offset, len));
}

/*
 * disk_size
 *
 * How large is the disk ?
 */
uint64_t
disk_size (disk_t *disk)
{
    return (((uint64_t)sector_count_total(disk)) * 
             (uint64_t)sector_size(disk));
}

/*
 * sector_size
 *
 * How large is our sector?
 */
uint32_t
sector_size (disk_t *disk)
{
    if (!disk || !disk->mbr || !disk->mbr->sector_size) {
        return (opt_sector_size);
    }

    return (disk->mbr->sector_size);
}

/*
 * cluster_size
 *
 * How large is our cluster?
 */
uint32_t
cluster_size (disk_t *disk)
{
    return (sector_size(disk) * disk->mbr->sectors_per_cluster);
}

/*
 * sector_reserved_count
 *
 * The first sector that contains a FAT.
 */
uint32_t
sector_reserved_count (disk_t *disk)
{
    return (disk->mbr->reserved_sector_count);
}

/*
 * sector_offset
 *
 * How much leading junk is there before the FAT filesystem. Usually this is
 * the boot block with partitions, grub etc...
 */
uint32_t
sector_offset (disk_t *disk)
{
    if (!disk || !sector_size(disk)) {
        ERR("Boot record, sector size is 0");
        return (0);
    }

    return ((uint32_t)(disk->offset / sector_size(disk)));
}

/*
 * sector_count_total
 *
 * How many sectors on the disk ?
 */
uint64_t
sector_count_total (disk_t *disk)
{
    if (disk->mbr->sector_count_large) {
        return (disk->mbr->sector_count_large);
    }

    return (disk->mbr->sector_count);
}

/*
 * root_dir_size_sectors
 *
 * The size of the root directory in sectors.
 */
uint32_t
root_dir_size_sectors (disk_t *disk)
{
    return (root_dir_size_bytes(disk) + (sector_size(disk) - 1)) /
            sector_size(disk);
}

/*
 * total_clusters
 *
 * The total number of clusters:
 */
uint32_t
total_clusters (disk_t *disk)
{
    if (!disk->mbr->sectors_per_cluster) {
        return (0);
    }

    return (sector_count_data(disk) / disk->mbr->sectors_per_cluster);
}

/*
 * partition_table_read
 *
 * Read all partitions.
 */
boolean
partition_table_read (disk_t *disk)
{
    uint32_t i;

    for (i = 0; i < MAX_PARTITON; i++) {
        int64_t offset = (typeof(offset)) (PART_BASE + (sizeof(part_t) * i));
        int64_t amount = sizeof(part_t);

        disk->parts[i] =
            (part_t *) file_read_from(disk->filename, offset, amount);
    }

    return (true);
}

/*
 * partition_table_write
 ?
 * Write all partitions.
 */
boolean
partition_table_write (disk_t *disk)
{
    uint32_t i;

    for (i = 0; i < MAX_PARTITON; i++) {
        int64_t offset = (typeof(offset)) (PART_BASE + (sizeof(part_t) * i));
        int64_t amount = sizeof(part_t);

        if (file_write_at(disk->filename, offset,
                          (uint8_t *) disk->parts[i], amount) < 0) {
            ERR("failed writing partition info");
            return (false);
        }
    }

    return (true);
}

/*
 * partition_table_print
 *
 * Dump all partitions.
 */
boolean
partition_table_print (disk_t *disk)
{
    const part_t empty_partition = {0};
    uint32_t i;

    for (i = 0; i < MAX_PARTITON; i++) {
        int64_t offset = (typeof(offset)) (PART_BASE + (sizeof(part_t) * i));

        if (!disk->parts[i]) {
            continue;
        }

        if (!memcmp(&empty_partition, disk->parts[i], sizeof(part_t))) {
            continue;
        }

        if (opt_verbose) {
            OUT("Partition %" PRIu32 ", %" PRIu32 " bytes", i, 
                (uint32_t) sizeof(part_t));

            disk_hex_dump(disk, disk->parts[i], offset, sizeof(part_t));
        }

        OUT("Partition %" PRIu32 " info:", i);

        OUT("  %*s%" PRIu32 " (%s)", -OUTPUT_FORMAT_WIDTH,
            "bootable", disk->parts[i]->bootable,
            (disk->parts[i]->bootable & 0x80) ? "Yes"  : "No");

        OUT("  %*s%" PRIu32 " [%s]", -OUTPUT_FORMAT_WIDTH,
            "OS ID", disk->parts[i]->os_id,
            msdos_get_systype(disk->parts[i]->os_id));

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH,
            "LBA", disk->parts[i]->LBA);

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH,
            "sector start", SECTOR(disk->parts[i]->sector_start));

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH,
            "sector end", SECTOR(disk->parts[i]->sector_end));

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH,
            "sectors in partition",
            disk->parts[i]->sectors_in_partition);

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH,
            "cylinder start", CYLINDER(disk->parts[i]->sector_start,
                                       disk->parts[i]->cyl_start));

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH,
            "cylinder end", CYLINDER(disk->parts[i]->sector_end,
                                     disk->parts[i]->cyl_end));

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH,
            "head start", disk->parts[i]->head_start);

        OUT("  %*s%" PRIu32 "", -OUTPUT_FORMAT_WIDTH,
            "head end", disk->parts[i]->head_end);
    }

    return (true);
}

/*
 * cluster_to_sector
 *
 * Convert a cluster to a sector.
 */
uint32_t
cluster_to_sector (disk_t *disk, uint32_t cluster)
{
    uint32_t sector;

    sector = sector_first_data_sector(disk) +
                    (cluster * disk->mbr->sectors_per_cluster);

    return (sector);
}

/*
 * sector_cache_add
 *
 * Add a sector to the cache of sectors. We use this to speed up dirent reads.
 */
boolean
sector_cache_add (disk_t *disk, uint32_t sector, uint8_t *buf)
{
    uint32_t datalen;

#ifndef ENABLE_CACHING_OF_SECTORS
    return (false);
#endif

    tree_sector_cache_node *node;

    if (!disk->tree_sector_cache) {
        disk->tree_sector_cache = tree_alloc(TREE_KEY_INTEGER,
                                             "TREE ROOT: sector cache");
    }

    node = (typeof(node)) myzalloc(sizeof(*node), "TREE NODE: sector cache");
    node->tree.key = sector;

    datalen = sector_size(disk);

    if (!tree_insert(disk->tree_sector_cache, &node->tree.node)) {
        DIE("cache sector %" PRIu32 " fail", sector);
    }

    node->buf = (typeof(node->buf)) myzalloc(datalen, "sector cache");;
    memcpy(node->buf, buf, datalen);

    return (false);
}

/*
 * sectors_cache_add
 *
 * Add a contiguous block of sectors to the cache.
 */
void
sectors_cache_add (disk_t * disk, uint32_t sector, uint32_t count,
                   uint8_t *buf)
{
    uint32_t i;
    uint8_t *b;

    b = buf;

    for (i = 0; i < count; i++) {
        sector_cache_add(disk, sector + i, b);
        b += sector_size(disk);
    }
}

/*
 * sector_cache_find
 *
 * Find a sector in the cache.
 */
uint8_t *
sector_cache_find (disk_t *disk, uint32_t sector)
{
    tree_sector_cache_node *result;
    tree_sector_cache_node target;

    if (!disk->tree_sector_cache) {
        return (0);
    }

    memset(&target, 0, sizeof(target));
    target.tree.key = sector;

    result = (typeof(result)) tree_find(disk->tree_sector_cache,
                                        &target.tree.node);
    if (!result) {
        return (0);
    }

    return (result->buf);
}

/*
 * sector_cache_destroy
 *
 * Destroy all sectors. Anticipation is they've been written.
 */
void
sector_cache_destroy (disk_t *disk)
{
    tree_sector_cache_node *node;

    if (!disk->tree_sector_cache) {
        return;
    }

    TREE_WALK(disk->tree_sector_cache, node) {
        tree_remove(disk->tree_sector_cache, &node->tree.node);
        myfree(node->buf);
        myfree(node);
    }

    myfree(disk->tree_sector_cache);
    disk->tree_sector_cache = 0;
}

/*
 * sector_read
 *
 * Read a block of sectors from the disk or cache.
 */
uint8_t *
sector_read (disk_t *disk, uint32_t sector_, uint32_t count)
{
    uint32_t datalen = sector_size(disk);
    uint8_t *data;
    uint8_t *cached;
    uint64_t offset;
    uint32_t sector;
    uint32_t i;
    uint8_t *b;

    /*
     * If no sector is cached then read the whole lot in one go.
     */
    sector = sector_;
    for (i = 0; i < count; i++, sector++) {
        if (sector_cache_find(disk, sector)) {
            break;
        }
    }

    sector = sector_;

    if (i == count) {
        /*
         * Nothing was found in the cache. Read from the disk.
         */
        DBG4("Read sector block %" PRIu32 " .. %" PRIu32 "", sector,
             sector + count);

        offset = sector * datalen;

        data = disk_read_from(disk, offset, datalen * count);
        if (!data) {
            DIE("failed to read disk sector %" PRIu32 "", sector_);
        }

        /*
         * Add these sectors to the cache.
         */
        sectors_cache_add(disk, sector, count, data);

        return (data);
    }

    /*
     * Else read some sectors from the cache and some from the disk.
     */
    b = data = (typeof(data)) myzalloc(count * datalen, "sector read");

    DBG4("Read sectors %" PRIu32 " .. %" PRIu32 "", sector, sector + count);

    sector = sector_;

    for (i = 0; i < count; i++, sector++) {
        cached = sector_cache_find(disk, sector);
        if (cached) {
            /*
             * Read from cache.
             */
            DBG4("Read from sector cache %" PRIu32 "", sector);

            memcpy(b, cached, datalen);
        } else {
            /*
             * Read from disk.
             */
            DBG4("Read from sector %" PRIu32 "", sector);

            offset = sector * datalen;

            uint8_t *tmp = disk_read_from(disk, offset, datalen);
            memcpy(b, tmp, datalen);

            /*
             * Add to the cache.
             */
            sectors_cache_add(disk, sector, 1, tmp);
            myfree(tmp);
        }

        b += datalen;
    }

    return (data);
}

/*
 * sector_write
 *
 * Write a block of sectors.
 */
boolean
sector_write (disk_t *disk, uint32_t sector_, uint8_t *data,
              uint32_t count)
{
    tree_sector_cache_node *result;
    tree_sector_cache_node target;
    uint32_t datalen;
    boolean write;
    uint32_t i;
    uint8_t *b;
    uint64_t offset;
    uint32_t sector;
    boolean ret;

    b = data;
    ret = true;
    datalen = sector_size(disk);
    sector = sector_;

    DBG4("Write sector block %" PRIu32 " .. %" PRIu32 "", sector_,
         sector_ + count);

    /*
     * Write only changed sectors.
     */
    for (i = 0; i < count; i++, sector++) {
        memset(&target, 0, sizeof(target));
        target.tree.key = sector;

        /*
         * If we have a cached sector, update it.
         */
        result = (typeof(result)) tree_find(disk->tree_sector_cache,
                                            &target.tree.node);
        if (result) {
            /*
             * If there is a change from the cache, update and write.
             */
            verify(result->buf);

            if (memcmp(result->buf, b, datalen)) {
                /*
                 * Update cache.
                 */
                memcpy(result->buf, b, datalen);
                DBG4("Change, write to sector %" PRIu32 "", sector);

                /*
                 * Must update the disk.
                 */
                write = true;
            } else {
                /*
                 * No change, no need to do anything.
                 */
                DBG4("No change, skip write to sector %" PRIu32 "", sector);
                write = false;
            }
        } else {
            /*
             * Add to cache and write to disk.
             */
            DBG4("Not cached, write to sector %" PRIu32 " and cache it",
                 sector);

            sector_cache_add(disk, sector, b);
            write = true;
        }

        if (write) {
            /*
             * Write to disk.
             */
            offset = sector * datalen;

            if (!disk_write_at(disk, offset, b, datalen)) {
                ret = false;
            }
        }

        b += datalen;
    }

    return (ret);
}

/*
 * sector_pre_write_print_dirty_sectors
 *
 * Print s list of sectors that are different and need writing.
 */
boolean
sector_pre_write_print_dirty_sectors (disk_t *disk, uint32_t sector_,
                                      uint8_t *data, uint32_t count)
{
    tree_sector_cache_node *result;
    tree_sector_cache_node target;
    uint32_t datalen;
    uint32_t i;
    uint8_t *b;
    uint32_t sector;
    boolean ret;
    boolean first;

    if (!opt_debug) {
        return (true);
    }

    first = true;
    b = data;
    ret = true;
    datalen = sector_size(disk);
    sector = sector_;

    /*
     * Write only changed sectors.
     */
    for (i = 0; i < count; i++, sector++) {
        memset(&target, 0, sizeof(target));
        target.tree.key = sector;

        /*
         * If we have a cached sector, update it.
         */
        result = (typeof(result)) tree_find(disk->tree_sector_cache,
                                            &target.tree.node);
        if (result) {
            /*
             * If there is a change from the cache, update and write.
             */
            verify(result->buf);

            if (memcmp(result->buf, b, datalen)) {
                if (first) {
                    if (opt_debug) {
                        printf("Writing dirty sectors to disk");
                    }

                    first = false;
                }

                if (opt_debug) {
                    printf(", %" PRIu32 "", sector);
                }
            }
        }

        b += datalen;
    }

    if (opt_debug) {
        if (!first) {
            printf("\n");
        }
    }

    return (ret);
}

/*
 * sector_write_no_cache
 *
 * Write a block of sectors straight to disk. No cache.
 */
boolean
sector_write_no_cache (disk_t *disk, uint32_t sector, uint8_t *data,
                       uint32_t count)
{
    uint32_t datalen;
    uint64_t offset;

    datalen = sector_size(disk) * count;
    offset = sector * sector_size(disk);

    return (disk_write_at(disk, offset, data, datalen));
}

/*
 * cluster_read
 *
 * Read an entire cluster.
 */
uint8_t *
cluster_read (disk_t *disk, uint32_t cluster, uint32_t count)
{
    uint8_t *data;
    uint32_t amount;
    uint32_t sector;

    sector = sector_first_data_sector(disk) +
                    (cluster * disk->mbr->sectors_per_cluster);

    DBG4("Read from cluster %" PRIu32 " (sector %" PRIu32 ", count %"
         PRIu32 "", cluster, sector, count);

    amount = count * disk->mbr->sectors_per_cluster;

    data = sector_read(disk, sector, amount);

    return (data);
}

/*
 * cluster_write
 *
 * Write an entire cluster.
 */
boolean
cluster_write (disk_t *disk, uint32_t cluster, uint8_t *data,
               uint32_t count)
{
    uint32_t amount;
    uint32_t sector;
    boolean ret;

    DBG4("Write to cluster %" PRIu32 ", count %" PRIu32 "", cluster, count);

    sector = sector_first_data_sector(disk) +
                    (cluster * disk->mbr->sectors_per_cluster);
    amount = count * disk->mbr->sectors_per_cluster;

    ret = sector_write(disk, sector, data, amount);

    return (ret);
}

/*
 * cluster_write_no_cache
 *
 * Write an entire cluster, no caching
 */
boolean
cluster_write_no_cache (disk_t *disk, uint32_t cluster, uint8_t *data,
                        uint32_t count)
{
    uint32_t amount;
    uint32_t sector;
    boolean ret;

    DBG4("Write to cluster %" PRIu32 ", count %" PRIu32 "", cluster, count);

    sector = sector_first_data_sector(disk) +
                    (cluster * disk->mbr->sectors_per_cluster);
    amount = count * disk->mbr->sectors_per_cluster;

    ret = sector_write_no_cache(disk, sector, data, amount);

    return (ret);
}
