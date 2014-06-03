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
#include <unistd.h>
#include <signal.h>

#include <libgen.h>
#include <fcntl.h>

#include "main.h"
#include "disk.h"
#include "command.h"

/*
 * Global options
 */
boolean opt_verbose;
boolean opt_quiet;
boolean opt_debug;
boolean opt_debug2;
boolean opt_debug3;
boolean opt_debug4;
boolean opt_debug5;

/*
 * Most common sector size.
 */
uint32_t opt_sector_size = DEFAULT_SECTOR_SIZE;

/*
 * Sectors per cluster.
 */
uint32_t opt_sectors_per_cluster;

/*
 * Die and print usage message.
 */
boolean die_with_usage;

/*
 * Tool usage.
 */
void usage (void)
{
    fprintf(stderr,
"fatdisk, version " VERSION "\n\n"

"fatdisk is a utility that allows you to perform various operations on files\n"
"on a DOS formatted disk image in FAT12,16,32 formats without needing to do any\n"
"mounting of the disk image, or needing root or sudo access.\n"
"\n"
"It can extract files from the DOS disk to the local harddrive, and likewise\n"
"can import files from the local disk back onto the DOS disk. Additionally\n"
"you can do basic operations like list, cat, hexdump etc...\n"
"\n"
"Lastly this tool can also format and partition a disk, setting up the FAT\n"
"filesystem and even copying in a bootloader like grub. This is a bit \n"
"experimental so use with care.\n"
"\n"
"You may specify the partition of the disk the tool is to look for, but it\n"
"will default to partition 0 if not. And if no partition info is found, it\n"
"will do a hunt of the disk to try and find it.\n"
"\n"
"Usage: fatdisk [OPTIONS] disk-image-file [COMMAND]\n"
"\n"
"COMMAND are things like extract, rm, ls, add, info, summary, format.\n"
);

    fprintf(stderr, "Options:\n");
    fprintf(stderr, "        --verbose        : print lots of disk info\n");
    fprintf(stderr, "        -verbose         :\n");
    fprintf(stderr, "        -v               :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        --quiet          : print less info when adding/removing\n");
    fprintf(stderr, "        -quiet           :\n");
    fprintf(stderr, "        -q               :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        --debug          : print internal debug info\n");
    fprintf(stderr, "        -debug           :\n");
    fprintf(stderr, "        -d               :\n");
    fprintf(stderr, "        -dd              : more debugs\n");
    fprintf(stderr, "        -ddd             : yet more debugs\n");
    fprintf(stderr, "        -dddd            : and still more\n");
    fprintf(stderr, "        -ddddd           : insane amount of debugs\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        --offset         : offset to start of disk\n");
    fprintf(stderr, "        -offset          : e.g. -o 32256, -o 63s -o 0x7e00\n");
    fprintf(stderr, "        -o               :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        --partition      : partition to use\n");
    fprintf(stderr, "        -partition       :\n");
    fprintf(stderr, "        -p               :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        --sectors_per_cluster : sectors per cluster\n");
    fprintf(stderr, "        -sectors_per_cluster  :\n");
    fprintf(stderr, "        -S               :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        --sectorsize     : default 512\n");
    fprintf(stderr, "        -sectorsize      :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        --help           : this help\n");
    fprintf(stderr, "        -help            :\n");
    fprintf(stderr, "        -h               :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        --version        : tool version number\n");
    fprintf(stderr, "        -version         :\n");
    fprintf(stderr, "        -ver             :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        info             : print disk info\n");
    fprintf(stderr, "        i                :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        summary          : print disk info summary\n");
    fprintf(stderr, "        sum              :\n");
    fprintf(stderr, "        s                :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        list      <pat>  : list a file or dir\n");
    fprintf(stderr, "        ls        <pat>  :\n");
    fprintf(stderr, "        l         <pat>  :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        find      <pat>  : find and raw list files\n");
    fprintf(stderr, "        fi        <pat>  :\n");
    fprintf(stderr, "        f         <pat>  :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        extract   <pat>  : extract a file or dir\n");
    fprintf(stderr, "        x         <pat>  :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        add       <pat>  : add a file or dir, keeping same\n");
    fprintf(stderr, "        a         <pat>  : full pathname on the disk image\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        fileadd   local-name [remote-name]\n");
    fprintf(stderr, "                         : add a file with a different name from source\n");
    fprintf(stderr, "        f         <pat>  :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        remove    <pat>  : remove a file or dir\n");
    fprintf(stderr, "        rm        <pat>  :\n");
    fprintf(stderr, "        r         <pat>  :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        hexdump          : include hex dump of files\n");
    fprintf(stderr, "        hex              :\n");
    fprintf(stderr, "        h                :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        cat              : raw dump of file to console\n");
    fprintf(stderr, "        ca               :\n");
    fprintf(stderr, "        c                :\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "        format\n");
    fprintf(stderr, "               size xG/xM\n");
    fprintf(stderr, "               [part 0-3]           select partiton\n");
    fprintf(stderr, "               [zero]               zero sectors\n");
    fprintf(stderr, "               [bootloader <file>]  install bootloader\n");
    fprintf(stderr, "               [<disktype>]         select filesys type\n");
    fprintf(stderr, "               where <disktype> is: ");

    uint32_t i;

    for (i = 0; i < 0xff; i++) {
        if (*msdos_get_systype(i)) {
            fprintf(stderr, "%s ", msdos_get_systype(i));
        }
    }

    fprintf(stderr, "\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  $ fatdisk mybootdisk ls\n");
    fprintf(stderr, "    ----daD            0       2013 Jan 02   locale/             LOCALE\n");
    fprintf(stderr, "    -----aD        18573       2013 Jan 02    ast.mo             AST.MO\n");
    fprintf(stderr, "    ...\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  $ fatdisk mybootdisk info\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  $ fatdisk mybootdisk summary\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  $ fatdisk mybootdisk extract dir\n");
    fprintf(stderr, "\t\t\t\t\t-- dumps dir to the local disk\n");
    fprintf(stderr, "  $ fatdisk mybootdisk rm dir\n");
    fprintf(stderr, "\t\t\t\t\t-- recursively remove dir\n");
    fprintf(stderr, "  $ fatdisk mybootdisk rm dir/*/*.c\n");
    fprintf(stderr, "\t\t\t\t\t-- selectively remove files\n");
    fprintf(stderr, "  $ fatdisk mybootdisk add dir\n");
    fprintf(stderr, "\t\t\t\t\t-- recursively add dir to the disk\n");
    fprintf(stderr, "  $ fatdisk mybootdisk hexdump foo.c\n");
    fprintf(stderr, "\t\t\t\t\t-- dump a file from the disk\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  $ fatdisk mybootdisk format size 1G name MYDISK part 0 50%% \\\n");
    fprintf(stderr, "      bootloader grub_disk part 1 50%% fat32 bootloader grub_disk\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t\t\t\t\t-- create and format a 1G disk\n");
    fprintf(stderr, "\t\t\t\t\t   with 2 FAT 32 partitions and grub\n");
    fprintf(stderr, "\t\t\t\t\t   installed in sector 0 of part 0\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Written by Neil McGill, goblinhack@gmail.com, with special thanks\n");
    fprintf(stderr, "to Donald Sharp, Andy Dalton and Mike Woods\n");
    fprintf(stderr, "\nfatdisk, version " VERSION "\n\n");
}

/*
 * fatdisk_version
 *
 * Version number
 */
static void fatdisk_version (void)
{
    fprintf(stderr, "fatdisk, version " VERSION "\n");
}

/*
 * Cleanup operations on exit.
 */
void quit (void)
{
    static boolean quitting;

    if (quitting) {
        return;
    }

    quitting = true;

    ptrcheck_fini();
}

void die (void)
{
    quit();

    exit(1);
}

static disk_t *disk;

static void killed (int sig)
{
    if (disk) {
        disk_command_close(disk);
        disk = 0;
    }

    exit(2);
}

/*
 * command_format
 *
 * Format and partition a disk.
 */
static void command_format (int32_t argc,
                            int64_t opt_disk_start_offset,
                            boolean opt_disk_start_offset_set,
                            int32_t arg,
                            char *argv[],
                            const char *opt_filename)
{
    uint64_t opt_disk_bootloader_size[MAX_PARTITON] = {0};
    boolean opt_disk_partition_used[MAX_PARTITON] = {0};
    uint64_t opt_disk_partition_size[MAX_PARTITON] = {0};
    uint32_t opt_disk_sector_start[MAX_PARTITON] = {0};
    uint64_t opt_disk_command_format_size = 0;
    uint32_t opt_disk_sector_end[MAX_PARTITON] = {0};
    char *opt_disk_bootloader[MAX_PARTITON] = {0};
    uint8_t opt_disk_os_id_default = DISK_FAT32;
    uint8_t opt_disk_os_id[MAX_PARTITON] = {0};
    boolean opt_disk_zero[MAX_PARTITON] = {0};
    char *opt_disk_command_format_name = 0;
    uint32_t last_partition;
    uint64_t needed_size;
    uint64_t total_size;
    uint32_t partition;
    uint64_t size;
    int32_t i;

    partition = 0;

    /*
     * Parse format args
     */
    for (i = arg; i < argc; i++) {
        /*
         * Size
         */
        if (!strcmp(argv[i], "size")) {

            if (i + 1 >= argc) {
                DIE("no size value");
            }

            if (strcasestr(argv[i + 1], "0x")) {
                opt_disk_command_format_size = strtol(argv[i + 1] + 2, 0, 16);
            } else {
                opt_disk_command_format_size = strtol(argv[i + 1], 0, 10);
            }

            if (strcasestr(argv[i + 1], "G")) {
                opt_disk_command_format_size *= (uint64_t) 1024 * 1024 * 1024;
            } else if (strcasestr(argv[i + 1], "M")) {
                opt_disk_command_format_size *= ONE_MEG;
            } else if (strcasestr(argv[i + 1], "K")) {
                opt_disk_command_format_size *= ONE_K;
            } else if (strcasestr(argv[i + 1], "s")) {
                opt_disk_command_format_size *= opt_sector_size;
            }

            i++;

            continue;
        }

        /*
         * Name
         */
        if (!strcmp(argv[i], "name")) {

            if (i + 1 >= argc) {
                DIE("no volume name");
            }

            opt_disk_command_format_name = dupstr(argv[i + 1], __FUNCTION__);

            i++;

            continue;
        }

        /*
         * Partition option
         */
        if (!strcmp(argv[i], "part")) {

            if (i + 1 >= argc) {
                DIE("no partition value");
            }

            i++;

            if (strcasestr(argv[i], "0x")) {
                partition = (uint32_t) strtol(argv[i] + 2, 0, 16);
            } else {
                partition = (uint32_t) strtol(argv[i], 0, 10);
            }

            if (partition >= MAX_PARTITON) {
                DIE("partition value invalid");
            }

            opt_disk_partition_used[partition] = true;

            continue;
        }

        /*
         * Zero disk sectors
         */
        if (!strcmp(argv[i], "zero")) {
            opt_disk_zero[partition] = true;
            continue;
        }

        /*
         * Bootloader option
         */
        if (!strcmp(argv[i], "bootloader") ||
            !strcmp(argv[i], "boot") ||
            !strcmp(argv[i], "b")) {

            if (i + 1 >= argc) {
                DIE("no bootloader value");
            }

            opt_disk_bootloader[partition] = argv[i + 1];

            i++;
            if (!file_exists(opt_disk_bootloader[partition])) {
                DIE("bootloader boot file %s does not exisst",
                    opt_disk_bootloader[partition]);
            }

            opt_disk_bootloader_size[partition] =
                file_size(opt_disk_bootloader[partition]);

            opt_disk_bootloader_size[partition] += opt_sector_size - 1;
            opt_disk_bootloader_size[partition] /= opt_sector_size;

            if (opt_disk_bootloader_size[partition] < 63) {
                opt_disk_bootloader_size[partition] = 63;
            }

            opt_disk_bootloader_size[partition] *= opt_sector_size;

            continue;
        }

        /*
         * Disk type
         */
        uint8_t os_id = msdos_parse_systype(argv[i]);

        if (os_id != 0xff) {
            opt_disk_os_id_default = os_id;

            opt_disk_os_id[partition] = opt_disk_os_id_default;

            continue;
        }

        if (strcasestr(argv[i], "0x")) {
            size = strtol(argv[i] + 2, 0, 16);
        } else {
            size = strtol(argv[i], 0, 10);

            if (!strcasestr(argv[i], "%")) {
                if (size <= 100) {
                    size = (uint32_t)
                        (((float)opt_disk_command_format_size / 100) *
                            (float)size);
                }
            }
        }

        if (strcasestr(argv[i], "G")) {
            size *= ONE_GIG;
        } else if (strcasestr(argv[i], "M")) {
            size *= ONE_MEG;
        } else if (strcasestr(argv[i], "K")) {
            size *= ONE_K;
        } else if (strcasestr(argv[i], "s")) {
            size *= opt_sector_size;
        } else if (strcasestr(argv[i], "%")) {
            size = strtol(argv[i], 0, 10);
            size = (uint32_t)
                (((float)opt_disk_command_format_size / 100) * (float)size);
        }

        if (size) {
            size += opt_sector_size - 1;
            size /= opt_sector_size;
            size *= opt_sector_size;

            opt_disk_partition_size[partition] = size;

            continue;
        }

        /*
         * Bad argument.
         */
        if (argv[i][0] == '-') {
            die_with_usage = true;
            DIE("unknown format argument, %s", argv[i]);
        }

        DIE("unknown format argument, %s", argv[i]);
    }

    /*
     * If we have a device and no size given, get its size.
     */
    if (!opt_disk_command_format_size) {
        /*
         * If the device exists?
         */
        opt_disk_command_format_size = file_size(opt_filename);
        if (opt_disk_command_format_size < DEFAULT_DISK_SIZE) {
            opt_disk_command_format_size = DEFAULT_DISK_SIZE;
        }
    }

    /*
     * Still no size? Guess.
     */
    if (!opt_disk_command_format_size) {
        opt_disk_command_format_size = DEFAULT_DISK_SIZE;
    }

    DBG4("opt_disk_command_format_size       %" PRIu64 " bytes "
         "%" PRIu64 "G %" PRIu64 "M %" PRIu64 " sectors",
         opt_disk_command_format_size,
         opt_disk_command_format_size / ONE_GIG,
         opt_disk_command_format_size / ONE_MEG,
         opt_disk_command_format_size / opt_sector_size);

    for (i = 0; i < MAX_PARTITON; i++) {
        DBG3("  Partition size            "
             "   %" PRIu64 " bytes %2.2fG %2.2fM %" PRIu64 " sectors",
             opt_disk_partition_size[i],
             (float)opt_disk_partition_size[i] / ONE_GIG,
             (float)opt_disk_partition_size[i] / ONE_MEG,
             opt_disk_partition_size[i] / opt_sector_size);
    }

    /*
     * Sanity checks.
     */
    total_size = opt_disk_command_format_size;
    last_partition = 0;
    needed_size = 0;

    /*
     * Have at least one partition? If not, make one.
     */
    uint32_t total_partitions = 0;

    for (i = 0; i < MAX_PARTITON; i++) {
        if (opt_disk_partition_used[i]) {
            total_partitions++;

            if (!opt_disk_os_id[i]) {
                opt_disk_os_id[i] = opt_disk_os_id_default;
            }
        }
    }

    if (!total_partitions) {
        DBG4("Create one partition to span disk");

        opt_disk_partition_used[0] = true;
        opt_disk_partition_size[0] = opt_disk_command_format_size;
        opt_disk_os_id[0] = opt_disk_os_id_default;

        for (i = 0; i < MAX_PARTITON; i++) {
            DBG3("  Partition size            "
                "   %" PRIu64 " bytes %2.2fG %2.2fM %" PRIu64 " sectors",
                opt_disk_partition_size[i],
                (float)opt_disk_partition_size[i] / ONE_GIG,
                (float)opt_disk_partition_size[i] / ONE_MEG,
                opt_disk_partition_size[i] / opt_sector_size);
        }
    }

    /*
     * Any without a size? Give them a fraction of what remains.
     */
    uint64_t allocated = total_size;
    uint32_t unsized_partitions = 0;

    DBG4("Check all clusters are allocated for size %" PRIu64 "", allocated);

    for (i = 0; i < MAX_PARTITON; i++) {
        if (opt_disk_partition_used[i]) {
            if (!opt_disk_partition_size[i]) {
                unsized_partitions++;
            } else {
                allocated -= opt_disk_partition_size[i];
            }
        }
    }

    DBG4("Remaining unallocated %" PRIu64 ", unsized_partitions %u",
        allocated, unsized_partitions);

    if (allocated && unsized_partitions) {
        DBG4("%u partitions have no size", unsized_partitions);

        for (i = 0; i < MAX_PARTITON; i++) {
            if (opt_disk_partition_size[i]) {
                continue;
            }

            if (opt_disk_partition_used[i]) {
                DBG4("Set unsized partiton %u to size %" PRIu64 "",
                     i, allocated / unsized_partitions);

                opt_disk_partition_size[i] = allocated / unsized_partitions;
            }
        }

        for (i = 0; i < MAX_PARTITON; i++) {
            DBG4("  Partition size            "
                 "   %" PRIu64 " bytes %2.2fG %2.2fM %" PRIu64 " sectors",
                 opt_disk_partition_size[i],
                 (float)opt_disk_partition_size[i] / ONE_GIG,
                 (float)opt_disk_partition_size[i] / ONE_MEG,
                 opt_disk_partition_size[i] / opt_sector_size);
        }
    }

    /*
     * How big a disk do we need?
     */
    for (i = 0; i < MAX_PARTITON; i++) {
        if (!opt_disk_partition_size[i]) {
            continue;
        }

        needed_size += opt_disk_partition_size[i];

        last_partition = 0;
    }

    /*
     * Check the needed size is not beyond the disk size.
     */
    if (needed_size > total_size) {
        OUT("The total byte size asked for in partitions, %" PRIu64 ", "
            "exceeds total on disk %" PRIu64 ", truncating "
            "partition %" PRIu32 " to fit...",
            needed_size, total_size, last_partition);

        while (needed_size > total_size) {
            opt_disk_partition_size[last_partition] -= opt_sector_size;
            needed_size -= opt_sector_size;
        }
    }

    uint32_t sector_start = 0;

    for (i = 0; i < MAX_PARTITON; i++) {
        if (!opt_disk_partition_size[i]) {
            continue;
        }

        opt_disk_sector_start[i] = sector_start;

        opt_disk_sector_end[i] =
                opt_disk_sector_start[i] +
                (uint32_t)
                (opt_disk_partition_size[i] / opt_sector_size);

        opt_disk_sector_end[i]--;

        sector_start += opt_disk_partition_size[i] / opt_sector_size;
    }

    for (i = 0; i < MAX_PARTITON; i++) {
        if (!opt_disk_partition_size[i]) {
            continue;
        }

        if (opt_quiet) {
            continue;
        }

        OUT("Partition %" PRIu32 ":", i);

        OUT("  Partition size            "
            "   %" PRIu64 " bytes %2.2fG %2.2fM %" PRIu64 " sectors",
            opt_disk_partition_size[i],
            (float)opt_disk_partition_size[i] / ONE_GIG,
            (float)opt_disk_partition_size[i] / ONE_MEG,
            opt_disk_partition_size[i] / opt_sector_size);

        OUT("  Sector start                 %-10" PRIu32 
            " (0x%" PRIx32 ")",
            opt_disk_sector_start[i],
            opt_disk_sector_start[i]);

        OUT("  Sector end                   %-10" PRIu32
            " (0x%" PRIx32 ")",
            opt_disk_sector_end[i],
            opt_disk_sector_end[i]);

        if (opt_disk_bootloader[i]) {
            OUT("  Bootloader boot file         %s",
                opt_disk_bootloader[i]);
        }

        if (opt_disk_bootloader_size[i]) {
            OUT("  Bootloader size          "
                "    %" PRIu64 " bytes %2.2fG %2.2fM %" PRIu64 " sectors",
                opt_disk_bootloader_size[i],
                (float)opt_disk_bootloader_size[i] / ONE_GIG,
                (float)opt_disk_bootloader_size[i] / ONE_MEG,
                opt_disk_bootloader_size[i] / opt_sector_size);
        }

        OUT("  OS ID                        %" PRIu32 " (%s)",
            opt_disk_os_id[i],
            msdos_get_systype(opt_disk_os_id[i]));
    }

    if (!file_is_special(opt_filename)) {
        unlink(opt_filename);
    }

    if (file_write(opt_filename, 0, 0) < 0) {
        DIE("Cannot write to %s", opt_filename);
    }

    unsigned char tmp = '\0';

    if (file_write_at(opt_filename, 
                       opt_disk_command_format_size - 1,
                       &tmp, 
                       (uint32_t) sizeof(tmp))) {
        DIE("Cannot write to end of file of %s", opt_filename);
    }

    /*
     * Format each partition.
     */
    for (i = 0; i < MAX_PARTITON; i++) {
        if (!opt_disk_partition_size[i]) {
            continue;
        }

        disk = disk_command_format(opt_filename,
                           i,
                           opt_disk_start_offset,
                           opt_disk_start_offset_set,
                           opt_disk_command_format_size,
                           opt_disk_command_format_name,
                           opt_disk_sector_start[i],
                           opt_disk_sector_end[i],
                           opt_disk_os_id[i],
                           opt_disk_zero[i],
                           opt_disk_bootloader[i],
                           opt_disk_bootloader_size[i]);
        if (!disk) {
            DIE("format of partition %" PRIu32 " failed", i);
        }

        disk_command_close(disk);
        disk = 0;
    }
}

/*
 * command_list
 *
 * Execute the list command
 */
static uint32_t command_list (int32_t argc, int32_t arg, char *argv[])
{
    uint32_t count;

    count = 0;

    if (arg == argc - 1) {
        /*
         * List files.
         */
        count += disk_command_list(disk, 0);
    } else {
        for (++arg; arg < argc; arg++) {
            /*
             * List files.
             */
            count += disk_command_list(disk, argv[arg]);
        }
    }

    if (!opt_quiet) {
        if (count == 1) {
            printf("Listed %" PRIu32 " entry\n", count);
        } else {
            printf("Listed %" PRIu32 " entries\n", count);
        }
    }

    return (count);
}

/*
 * command_find
 *
 * Execute the find command
 */
static uint32_t command_find (int32_t argc, int32_t arg, char *argv[])
{
    uint32_t count;

    count = 0;

    if (arg == argc - 1) {
        /*
         * find files.
         */
        count += disk_command_find(disk, 0);
    } else {
        for (++arg; arg < argc; arg++) {
            /*
             * find files.
             */
            count += disk_command_find(disk, argv[arg]);
        }
    }

    if (opt_verbose) {
        if (count == 1) {
            printf("Found %" PRIu32 " entry\n", count);
        } else {
            printf("Found %" PRIu32 " entries\n", count);
        }
    }

    return (count);
}

/*
 * command_hexdump
 *
 * Execute the hexdump command
 */
static uint32_t command_hexdump (int32_t argc, int32_t arg, char *argv[])
{
    uint32_t count;

    count = 0;

    if (arg == argc - 1) {
        /*
         * Dump all files.
         */
        count += disk_command_hex_dump(disk, 0);
    } else {
        for (++arg; arg < argc; arg++) {
            /*
             * Dump specific files.
             */
            count += disk_command_hex_dump(disk, argv[arg]);
        }
    }

    if (!opt_quiet) {
        if (count == 1) {
            printf("Dumped %" PRIu32 " entry\n", count);
        } else {
            printf("Dumped %" PRIu32 " entries\n", count);
        }
    }

    return (count);
}

/*
 * command_cat
 *
 * Execute the cat command
 */
static uint32_t command_cat (int32_t argc, int32_t arg, char *argv[])
{
    uint32_t count;

    count = 0;

    if (arg == argc - 1) {
        /*
         * Dump all files.
         */
        count += disk_command_cat(disk, 0);
    } else {
        for (++arg; arg < argc; arg++) {
            /*
             * Dump specific files.
             */
            count += disk_command_cat(disk, argv[arg]);
        }
    }

    return (count);
}

/*
 * command_extract
 *
 * Execute the extract command
 */
static uint32_t command_extract (int32_t argc, int32_t arg, char *argv[])
{
    uint32_t count;

    count = 0;

    if (arg == argc - 1) {
        /*
         * Extract all files.
         */
        count += disk_command_extract(disk, 0);
    } else {
        for (++arg; arg < argc; arg++) {
            /*
             * Extract specific files.
             */
            count += disk_command_extract(disk, argv[arg]);
        }
    }

    if (!opt_quiet) {
        if (count == 1) {
            printf("Extracted %" PRIu32 " entry\n", count);
        } else {
            printf("Extracted %" PRIu32 " entries\n", count);
        }
    }

    return (count);
}

/*
 * command_add
 *
 * Execute the add command
 */
static uint32_t command_add (int32_t argc, int32_t arg, char *argv[])
{
    uint32_t count;

    count = 0;

    if (arg == argc - 1) {
        /*
         * Add all files.
         */
        count += disk_add(disk, 0, 0);
    } else {
        for (++arg; arg < argc; arg++) {
            /*
             * Add specific files.
             */
            count += disk_add(disk,
                              argv[arg] /* source */,
                              argv[arg] /* target */);
        }
    }

    if (!opt_quiet) {
        if (count == 1) {
            printf("Added %" PRIu32 " entry\n", count);
        } else {
            printf("Added %" PRIu32 " entries\n", count);
        }
    }

    return (count);
}

/*
 * command_remove
 *
 * Execute the remove command
 */
static uint32_t command_remove (int32_t argc, int32_t arg, char *argv[])
{
    uint32_t count;

    count = 0;

    if (arg == argc - 1) {
        /*
         * remove all files.
         */
        count += disk_command_remove(disk, 0);
    } else {
        for (++arg; arg < argc; arg++) {
            /*
             * remove specific files.
             */
            count += disk_command_remove(disk, argv[arg]);
        }
    }

    if (!opt_quiet) {
        if (count == 1) {
            printf("Removed %" PRIu32 " entry\n", count);
        } else {
            printf("Removed %" PRIu32 " entries\n", count);
        }
    }

    return (count);
}

/*
 * command_fileadd
 *
 * Execute the fileadd command
 */
static uint32_t command_fileadd (int32_t argc, int32_t arg, char *argv[])
{
    uint32_t count;

    count = 0;

    if (arg == argc - 1) {
        /*
         * Add all files.
         */
        count += disk_addfile(disk, 0, 0);
    } else {
        for (++arg; arg < argc; arg++) {
            /*
             * Add specific files.
             */
            if (arg + 1 >= argc) {
                /*
                 * Add the full path name source to the basename target if
                 * we are at the end of the args.
                 */
                char *b = mybasename(argv[arg], __FUNCTION__);

                count += disk_addfile(disk, argv[arg], b);

                myfree(b);
            } else {
                /*
                 * Add the given source as the given target.
                 */
                count += disk_addfile(disk, argv[arg], argv[arg + 1]);

                arg++;
            }
        }
    }

    if (!opt_quiet) {
        if (count == 1) {
            printf("Added %" PRIu32 " entry\n", count);
        } else {
            printf("Added %" PRIu32 " entries\n", count);
        }
    }

    return (count);
}

/*
 * main
 *
 * Main entry point.
 */
int32_t main (int32_t argc, char *argv[])
{
    int64_t opt_disk_start_offset = 0;
    boolean opt_disk_start_offset_set = false;
    uint32_t opt_disk_partition = 0;
    boolean opt_disk_command_list_set = false;
    boolean opt_disk_command_find_set = false;
    boolean opt_disk_command_extract_set = false;
    boolean opt_disk_add_set = false;
    boolean opt_disk_file_add_set = false;
    boolean opt_disk_command_remove_set = false;
    boolean opt_disk_command_info_set = false;
    boolean opt_disk_command_summary_set = false;
    boolean opt_disk_command_hex_dump_set = false;
    boolean opt_disk_command_cat_set = false;
    boolean opt_disk_command_format_set = false;
    boolean opt_disk_partition_set = false;
    const char *opt_filename = 0;
    boolean command_set = false;
    int32_t i;

    ptrcheck_leak_snapshot();

    signal(SIGINT, killed);

#ifdef REGEXP_TEST
    regexp_test();
    exit(0);
#endif

    command_set = false;

    if (argc < 2) {
        die_with_usage = true;
        DIE("not enough arguments");
    }

    for (i = 1; i < argc; i++) {
        /*
         * --version
         */
        if (!strcmp(argv[i], "--version") ||
            !strcmp(argv[i], "-version") ||
            !strcmp(argv[i], "-ver")) {
            fatdisk_version();
            quit();
            exit(0);
        }

        /*
         * --help
         */
        if (!strcmp(argv[i], "--help") ||
            !strcmp(argv[i], "-help") ||
            !strcmp(argv[i], "-h")) {
            usage();
            quit();
            exit(0);
        }

        /*
         * --verbose
         */
        if (!strcmp(argv[i], "--verbose") ||
            !strcmp(argv[i], "-verbose") ||
            !strcmp(argv[i], "-v")) {

            opt_verbose = true;
            continue;
        }

        /*
         * --quiet
         */
        if (!strcmp(argv[i], "--quiet") ||
            !strcmp(argv[i], "-quiet") ||
            !strcmp(argv[i], "-q")) {

            opt_quiet = true;
            continue;
        }

        /*
         * --debug
         */
        if (!strcmp(argv[i], "--debug") ||
            !strcmp(argv[i], "-debug") ||
            !strcmp(argv[i], "-d")) {

            opt_debug = true;
            continue;
        }

        /*
         * --dd
         */
        if (!strcmp(argv[i], "--dd") ||
            !strcmp(argv[i], "-dd")) {

            opt_debug = true;
            opt_debug2 = true;
            continue;
        }

        /*
         * --ddd
         */
        if (!strcmp(argv[i], "--ddd") ||
            !strcmp(argv[i], "-ddd")) {

            opt_debug = true;
            opt_debug2 = true;
            opt_debug3 = true;
            continue;
        }

        /*
         * --dddd
         */
        if (!strcmp(argv[i], "--dddd") ||
            !strcmp(argv[i], "-dddd")) {

            opt_debug  = true;
            opt_debug2 = true;
            opt_debug3 = true;
            opt_debug4 = true;
            continue;
        }

        /*
         * --ddddd
         */
        if (!strcmp(argv[i], "--ddddd") ||
            !strcmp(argv[i], "-ddddd")) {

            opt_debug  = true;
            opt_debug2 = true;
            opt_debug3 = true;
            opt_debug4 = true;
            opt_debug5 = true;
            continue;
        }

        /*
         * Also support -vv style verbosity as I keep forgetting to use dd
         * given how often I use ssh with -vv.
         */

        /*
         * --vv
         */
        if (!strcmp(argv[i], "--vv") ||
            !strcmp(argv[i], "-vv")) {

            opt_debug = true;
            opt_debug2 = true;
            continue;
        }

        /*
         * --vvv
         */
        if (!strcmp(argv[i], "--vvv") ||
            !strcmp(argv[i], "-vvv")) {

            opt_debug = true;
            opt_debug2 = true;
            opt_debug3 = true;
            continue;
        }

        /*
         * --vvvv
         */
        if (!strcmp(argv[i], "--vvvv") ||
            !strcmp(argv[i], "-vvvv")) {

            opt_debug  = true;
            opt_debug2 = true;
            opt_debug3 = true;
            opt_debug4 = true;
            continue;
        }

        /*
         * --vvvvv
         */
        if (!strcmp(argv[i], "--vvvvv") ||
            !strcmp(argv[i], "-vvvvv")) {

            opt_debug  = true;
            opt_debug2 = true;
            opt_debug3 = true;
            opt_debug4 = true;
            opt_debug5 = true;
            continue;
        }

        /*
         * --offset
         */
        if (!strcmp(argv[i], "--offset") ||
            !strcmp(argv[i], "-offset") ||
            !strcmp(argv[i], "-o")) {

            if (i + 1 >= argc) {
                DIE("no offset value");
            }

            if (strcasestr(argv[i + 1], "0x")) {
                opt_disk_start_offset = strtol(argv[i + 1] + 2, 0, 16);
            } else if (strcasestr(argv[i + 1], "s")) {
                opt_disk_start_offset = strtol(argv[i + 1], 0, 10);
                opt_disk_start_offset *= opt_sector_size;
            } else {
                opt_disk_start_offset = strtol(argv[i + 1], 0, 10);
            }

            opt_disk_start_offset_set = true;

            i++;

            continue;
        }

        /*
         * --partition
         */
        if (!strcmp(argv[i], "--partition") ||
            !strcmp(argv[i], "-partition") ||
            !strcmp(argv[i], "-p")) {

            if (i + 1 >= argc) {
                DIE("no partition value");
            }

            if (strcasestr(argv[i + 1], "0x")) {
                opt_disk_partition = (uint32_t)
                    strtol(argv[i + 1] + 2, 0, 16);
            } else if (strcasestr(argv[i + 1], "s")) {
                opt_disk_partition = (uint32_t)
                    strtol(argv[i + 1], 0, 10);
                opt_disk_start_offset *= opt_sector_size;
            } else {
                opt_disk_partition = (uint32_t)
                    strtol(argv[i + 1], 0, 10);
            }

            opt_disk_partition_set = true;

            i++;

            continue;
        }

        /*
         * --sectorsize
         */
        if (!strcmp(argv[i], "--sectorsize") ||
            !strcmp(argv[i], "-sectorsize")) {

            if (i + 1 >= argc) {
                DIE("no sectorsize value");
            }

            if (strcasestr(argv[i + 1], "0x")) {
                opt_sector_size = (uint32_t)
                    strtol(argv[i + 1] + 2, 0, 16);
            } else {
                opt_sector_size = (uint32_t)
                    strtol(argv[i + 1], 0, 10);
            }

            i++;

            continue;
        }

        /*
         * --sectors_per_cluster
         */
        if (!strcmp(argv[i], "--sectors_per_cluster") ||
            !strcmp(argv[i], "-sectors_per_cluster")) {

            if (i + 1 >= argc) {
                DIE("no sectors_per_cluster value");
            }

            if (strcasestr(argv[i + 1], "0x")) {
                opt_sectors_per_cluster = (uint32_t)
                    strtol(argv[i + 1] + 2, 0, 16);
            } else {
                opt_sectors_per_cluster = (uint32_t)
                    strtol(argv[i + 1], 0, 10);
            }

            i++;

            continue;
        }

        /*
         * Bad argument.
         */
        if (argv[i][0] == '-') {
            die_with_usage = true;
            DIE("unknown argument, %s", argv[i]);
        }
    }

    for (i = 1; i < argc; i++) {
        /*
         * list
         */
        if (!strcmp(argv[i], "list") ||
            !strcmp(argv[i], "ls") ||
            !strcmp(argv[i], "l")) {

            if (command_set) {
                die_with_usage = true;
                DIE("command already set");
            }
            command_set = true;

            opt_disk_command_list_set = true;
            break;
        }

        /*
         * find
         */
        if (!strcmp(argv[i], "find") ||
            !strcmp(argv[i], "ls") ||
            !strcmp(argv[i], "l")) {

            if (command_set) {
                die_with_usage = true;
                DIE("command already set");
            }
            command_set = true;

            opt_disk_command_find_set = true;
            break;
        }

        /*
         * extract
         */
        if (!strcmp(argv[i], "extract") ||
            !strcmp(argv[i], "ex") ||
            !strcmp(argv[i], "x")) {

            if (command_set) {
                die_with_usage = true;
                DIE("command already set");
            }
            command_set = true;

            opt_disk_command_extract_set = true;
            break;
        }

        /*
         * add
         */
        if (!strcmp(argv[i], "add") ||
            !strcmp(argv[i], "ad") ||
            !strcmp(argv[i], "a")) {

            if (command_set) {
                die_with_usage = true;
                DIE("command already set");
            }
            command_set = true;

            opt_disk_add_set = true;
            break;
        }

        /*
         * fileadd
         */
        if (!strcmp(argv[i], "fileadd") ||
            !strcmp(argv[i], "addfile") ||
            !strcmp(argv[i], "file") ||
            !strcmp(argv[i], "f")) {

            if (command_set) {
                die_with_usage = true;
                DIE("command already set");
            }
            command_set = true;

            opt_disk_file_add_set = true;
            break;
        }

        /*
         * remove
         */
        if (!strcmp(argv[i], "remove") ||
            !strcmp(argv[i], "rm") ||
            !strcmp(argv[i], "r")) {

            if (command_set) {
                die_with_usage = true;
                DIE("command already set");
            }
            command_set = true;

            opt_disk_command_remove_set = true;
            break;
        }

        /*
         * info
         */
        if (!strcmp(argv[i], "info") ||
            !strcmp(argv[i], "in") ||
            !strcmp(argv[i], "i")) {

            if (command_set) {
                die_with_usage = true;
                DIE("command already set");
            }
            command_set = true;

            opt_disk_command_info_set = true;
            break;
        }

        /*
         * summary
         */
        if (!strcmp(argv[i], "summary") ||
            !strcmp(argv[i], "summ") ||
            !strcmp(argv[i], "sum") ||
            !strcmp(argv[i], "su") ||
            !strcmp(argv[i], "s")) {

            if (command_set) {
                die_with_usage = true;
                DIE("command already set");
            }
            command_set = true;

            opt_disk_command_summary_set = true;
            break;
        }

        /*
         * format
         */
        if (!strcmp(argv[i], "format")) {

            if (command_set) {
                die_with_usage = true;
                DIE("command already set");
            }
            command_set = true;

            opt_disk_command_format_set = true;
            break;
        }

        /*
         * hexdump
         */
        if (!strcmp(argv[i], "hexdump") ||
            !strcmp(argv[i], "hex") ||
            !strcmp(argv[i], "he") ||
            !strcmp(argv[i], "h")) {

            if (command_set) {
                die_with_usage = true;
                DIE("command already set");
            }
            command_set = true;

            opt_disk_command_hex_dump_set = true;
            break;
        }

        /*
         * cat
         */
        if (!strcmp(argv[i], "cat") ||
            !strcmp(argv[i], "ca") ||
            !strcmp(argv[i], "c")) {

            if (command_set) {
                die_with_usage = true;
                DIE("command already set");
            }
            command_set = true;

            opt_disk_command_cat_set = true;
            break;
        }

        /*
         * Bad argument.
         */
        if (command_set) {
            if (argv[i][0] == '-') {
                die_with_usage = true;
                DIE("unknown argument, %s", argv[i]);
            }
        }
    }

    if (i == 1) {
        die_with_usage = true;
        DIE("no disk image specified");
    }

    opt_filename = argv[i - 1];

    if (!command_set) {
        die_with_usage = true;
        DIE("Please specify a command after the disk name");
    }

    if (opt_disk_command_format_set) {
        command_format(argc,
                       opt_disk_start_offset,
                       opt_disk_start_offset_set,
                       i + 1, argv, opt_filename);

        opt_disk_command_summary_set = true;
    }

    /*
     * Check the disk file exists.
     */
    if (!file_exists(opt_filename)) {
        DIE("Disk image file %s does not exist", opt_filename);
    }

    /*
     * If not given an offset, try and find a viable DOS disk by scanning
     * the file.
     */
    if (!opt_disk_start_offset_set) {
        opt_disk_start_offset = disk_command_query(opt_filename,
                                                   opt_disk_partition,
                                                   opt_disk_partition_set,
                                                   false /* hunt */);
    }

    /*
     * Query the disk.
     */
    disk = disk_command_open(opt_filename,
                             opt_disk_start_offset,
                             opt_disk_partition,
                             true);
    if (!disk) {
        DIE("disk open of %s failed", opt_filename);
    }

    /*
     * Command: info
     */
    if (opt_disk_command_info_set) {
        disk_command_info(disk);
    }

    /*
     * Command: summary
     */
    if (opt_disk_command_summary_set) {
        disk_command_summary(disk, opt_filename,
                             opt_disk_partition_set,
                             opt_disk_partition);
    }

    /*
     * Command: list
     */
    if (opt_disk_command_list_set) {
        (void) command_list(argc, i, argv);
    }

    /*
     * Command: find
     */
    if (opt_disk_command_find_set) {
        (void) command_find(argc, i, argv);
    }

    /*
     * Command: hexdump
     */
    if (opt_disk_command_hex_dump_set) {
        (void) command_hexdump(argc, i, argv);
    }

    /*
     * Command: cat
     */
    if (opt_disk_command_cat_set) {
        (void) command_cat(argc, i, argv);
    }

    /*
     * Command: extract
     */
    if (opt_disk_command_extract_set) {
        (void) command_extract(argc, i, argv);
    }

    /*
     * Command: add
     */
    if (opt_disk_add_set) {
        (void) command_add(argc, i, argv);
    }

    /*
     * Command: file_add
     */
    if (opt_disk_file_add_set) {
        (void) command_fileadd(argc, i, argv);
    }

    /*
     * Command: remove
     */
    if (opt_disk_command_remove_set) {
        (void) command_remove(argc, i, argv);
    }

    /*
     * Default action.
     */
    if (!command_set) {
        if (!opt_disk_command_find_set &&
            !opt_disk_command_list_set &&
            !opt_disk_command_extract_set &&
            !opt_disk_add_set &&
            !opt_disk_file_add_set &&
            !opt_disk_command_remove_set &&
            !opt_disk_command_info_set) {
            (void) disk_command_list(disk, 0);
        }
    }

    disk_command_close(disk);
    disk = 0;

    quit();

    return (0);
}
