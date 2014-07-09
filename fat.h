/*
 * Copyright (C) 2013 Neil McGill
 *
 * See the LICENSE file for license.
 */

boolean fat_format(disk_t *disk, uint32_t partition, uint32_t os_id);
uint32_t fat_type(disk_t *disk);
uint32_t disk_walk(disk_t *disk,
                   const char *filter_,
                   const char *dir_name_,
                   uint32_t cluster,
                   uint32_t parent_cluster,
                   uint32_t depth,
                   disk_walk_args_t *args);
uint32_t
disk_command_add_file_or_dir(disk_t *disk,
                             const char *source_file_or_dir,
                             const char *target_file_or_dir,
                             boolean addfile);
void fat_read(disk_t *disk);
void fat_write(disk_t *disk);
uint64_t fat_size_bytes(disk_t *disk);
uint64_t fat_size_sectors(disk_t *disk);
uint64_t cluster_how_many_free(disk_t *disk);
