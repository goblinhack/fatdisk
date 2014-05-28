/*
 * Copyright (C) 2013 Neil McGill
 *
 * See the LICENSE file for license.
 */

disk_t *disk_command_open(const char *filename,
                          int64_t offset,
                          uint32_t opt_disk_partition,
                          boolean opt_disk_partition_set);

disk_t *disk_command_format(const char *filename,
                    uint32_t partition,
                    int64_t opt_disk_start_offset,
                    boolean opt_disk_start_offset_set,
                    uint64_t opt_disk_command_format_size,
                    const char *name,
                    uint32_t opt_disk_sector_start,
                    uint32_t opt_disk_sector_end,
                    uint8_t opt_disk_os_id,
                    boolean zero_sectors,
                    const char *opt_disk_mbr,
                    uint64_t opt_disk_mbr_size);

boolean disk_command_summary(disk_t *, const char *filename,
                     boolean partition_set,
                     uint32_t partition);

uint64_t disk_command_query(const char *filename,
                            uint32_t partition,
                            boolean partiton_set,
                            boolean hunt);
boolean disk_command_info(disk_t *);
uint32_t disk_command_list(disk_t *, const char *filter);
uint32_t disk_command_find(disk_t *, const char *filter);
uint32_t disk_command_hex_dump(disk_t *, const char *filter);
uint32_t disk_command_cat(disk_t *, const char *filter);
uint32_t disk_command_extract(disk_t *, const char *filter);
uint32_t disk_command_remove(disk_t *, const char *filter);
uint32_t disk_add(disk_t *, const char *filter, const char *add_as);
uint32_t disk_addfile(disk_t *, const char *filter, const char *add_as);
void disk_command_close(disk_t *);
