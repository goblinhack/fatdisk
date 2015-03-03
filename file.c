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
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <libgen.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/statvfs.h>

#include "main.h"

/*
 * Is this a device and not a file?
 */
boolean file_is_special (const char *filename)
{
    struct stat buf;

    if (!filename) {
        DIE("no filename");
    }

    if (stat(filename, &buf) >= 0) {
        return (S_ISBLK(buf.st_mode));
    }

    return (false);
}

/*
 * How large is the file?
 */
int64_t file_size (const char *filename)
{
    struct stat buf;

    if (!filename) {
        DIE("no filename");
    }

    if (file_is_special(filename)) {
        struct statvfs stat;

        if (statvfs(filename, &stat) >= 0) {
            return (stat.f_blocks * stat.f_frsize);
        }
    }

    if (stat(filename, &buf) >= 0) {
        return (buf.st_size);
    }

    return (-1);
}

/*
 * How large is the file?
 */
static int64_t fd_file_size (int fd)
{
    int64_t size;
    
    (void) lseek(fd, 0L, SEEK_SET);

    size = (lseek(fd, (off_t) 0, SEEK_END)); 

    return (size);
}

unsigned char *file_read (const char *filename, int64_t *out_len)
{
    unsigned char *buffer;
    int64_t len;
    int fd;

    if (!filename) {
        DIE("no filename");
    }

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        DIE("Failed to open file \"%s\" for reading: %s",
            filename, strerror(errno));
        return (0);
    }

    /*
     * Get the file size.
     */
    if (lseek(fd, 0, SEEK_END) < 0) {
        ERR("Failed to seek end of file \"%s\": %s",
            filename, strerror(errno));
        close(fd);
        return (0);
    }

    len = fd_file_size(fd);
    if (len == -1) {
        ERR("Failed to get size of file \"%s\": %s",
            filename, strerror(errno));
        close(fd);
        return (0);
    }

    if (lseek(fd, 0, SEEK_SET) < 0) {
        ERR("Failed to seek begin of file \"%s\": %s",
            filename, strerror(errno));
        close(fd);
        return (0);
    }

    buffer = (unsigned char *)myzalloc((uint32_t)
                                       len + sizeof((char)'\0'),
                                       "file read");
    if (!buffer) {
        ERR("Failed to alloc mem for file \"%s\": %s",
            filename, strerror(errno));
        close(fd);
        return (0);
    }

    if (len && (read(fd, buffer, len) != len)) {
        ERR("Failed to read %" PRIu64 " bytes from file \"%s\": %s",
            len, filename, strerror(errno));
        close(fd);
        return (0);
    }

    if (out_len) {
        *out_len = len;
    }

    close(fd);

    return (buffer);
}

unsigned char *file_read_from (const char *filename,
                               int64_t offset,
                               int64_t len)
{
    unsigned char *buffer;
    int fd;

    if (!filename) {
        DIE("no filename");
    }

    if (!len) {
        DBG("Asked to read 0 bytes from \"%s\": %s",
            filename, strerror(errno));
        return (0);
    }

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        ERR("Failed to open file \"%s\" for reading: %s",
            filename, strerror(errno));
        return (0);
    }

    if (lseek(fd, offset, SEEK_SET) < 0) {
        ERR("Failed to seek to offset %" PRIu64 " \"%s\": %s",
            offset, filename, strerror(errno));
        close(fd);
        return (0);
    }

    buffer = (unsigned char *)myzalloc((uint32_t)
                                       len + sizeof((char)'\0'),
                                       "file read from");
    if (!buffer) {
        ERR("Failed to alloc mem for file \"%s\": %s",
            filename, strerror(errno));
        close(fd);
        return (0);
    }

    if (read(fd, buffer, len) != len) {
        ERR("Failed to read %" PRIu64 " bytes from file at "
            "offset %" PRIu64 " \"%s\": %s",
            len, offset, filename, strerror(errno));
        close(fd);
        return (0);
    }

    if (opt_debug5) {
        hex_dump(buffer, 0, len);
    }

    close(fd);

    return (buffer);
}

int64_t file_write (const char *filename, unsigned char *buffer, int64_t len)
{
    int fd;
    int rc;

    if (!filename) {
        DIE("no filename");
    }

    fd = open(filename, O_RDWR | O_CREAT, getumask());
    if (fd < 0) {
        DIE("Failed to open file \"%s\" for writing: %s",
            filename, strerror(errno));
        return (-1);
    }

    if (!buffer || !len) {
        close(fd);
        return (0);
    }

    rc = write(fd, buffer, len);

    /*
     * Check written one object.
     */
    if (rc < 0) {
        DIE("Failed to write to file \"%s\" len %" PRIu64 ": %s",
            filename, len, strerror(errno));
        close(fd);
        return (-1);
    }

    close(fd);
    return (0);
}

int64_t file_write_at (const char *filename,
                       int64_t offset,
                       unsigned char *buffer, int64_t len)
{
    int fd;
    int rc;

    if (!filename) {
        DIE("no filename");
    }

    fd = open(filename, O_WRONLY);
    if (fd < 0) {
        DIE("Failed to open file \"%s\" for writing: %s",
            filename, strerror(errno));
        return (-1);
    }

    (void)lseek(fd, 0L, SEEK_SET);

    if (lseek(fd, offset, SEEK_SET) < 0) {
        ERR("Failed to seek to offset %" PRIu64 " \"%s\": %s",
            offset, filename, strerror(errno));
        close(fd);
        return (-1);
    }

    rc = write(fd, buffer, len);

    /*
     * Check written one object.
     */
    if (rc < 0) {
        DIE("Failed to write to file \"%s\" offset %" PRIu64 " "
            "len %" PRIu64 ": %s",
            filename, offset, len, strerror(errno));
        close(fd);
        return (-1);
    }

    close(fd);
    return (0);
}

/*
 * Does the requested file exist?
 */
boolean file_exists (const char *filename)
{
    struct stat buf;

    if (!filename) {
        DIE("no filename");
    }

    if (stat(filename, &buf) >= 0) {
        if (S_ISDIR(buf. st_mode)) {
            return (0);
        }

        return (1);
    }

    return (0);
}

unsigned char *file_read_if_exists (const char *filename, int64_t *out_len)
{
    if (!filename) {
        DIE("no filename");
    }

    if (file_exists(filename)) {
        return (file_read(filename, out_len));
    }

    return (0);
}

/*
 * How large is the file?
 */
boolean file_mtime (const char *filename,
                    int32_t *day,
                    int32_t *month,
                    int32_t *year)
{
    struct stat buf;

    if (!filename) {
        DIE("no filename");
    }

    if (stat(filename, &buf) >= 0) {
        struct tm *mytime = localtime(&buf.st_mtime);

        *day = mytime->tm_mday;
        *month = mytime->tm_mon + 1;
        *year = mytime->tm_year + 1900;

        return (true);
    }

    return (false);
}

/*
 * Does the requested file exist?
 */
boolean file_non_zero_size_exists (const char *filename)
{
    if (!filename) {
        DIE("no filename");
    }

    if (!file_exists(filename)) {
        return (0);
    }

    if (!file_size(filename)) {
        return (0);
    }

    return (1);
}

/*
 * Get the file creation mask.
 */
uint32_t getumask (void)
{
    static boolean done;
    static mode_t mask;

    if (!done) {
        done = true;

        /*
         * Invert it.
         */
        mask = 0777 - umask(0);
    }

    return (mask);
}

boolean file_match (const char *regexp_in, const char *name_in, boolean is_dir)
{
    boolean match = false;
    char *top_level_name;
    char regexp[MAX_STR];
    boolean is_regexp;
    char *name;
    uint32_t c;

    if (!regexp_in) {
        return (true);
    }

    regexp[0] = 0;

    /*
     * Convert * to regexp pattern.
     */
    if (strisregexp(regexp_in)) {
        for (c = 0; c < strlen(regexp_in); c++) {
            char tmp2[2];

            if (c == 0) {
                if (regexp_in[0] != '^') {
                    tmp2[0] = '^';
                    tmp2[1] = 0;
                    strncat(regexp, tmp2,
                            sizeof(regexp) - strlen(regexp) - 1);
                }
            }

            if (regexp_in[c] == '*') {
                strncat(regexp, "[a-z0-9_-]*",
                        sizeof(regexp) - strlen(regexp) - 1);
            } else {
                tmp2[0] = regexp_in[c];
                tmp2[1] = 0;
                strncat(regexp, tmp2,
                        sizeof(regexp) - strlen(regexp) - 1);
            }
        }

        is_regexp = true;
    } else {
#if 0
        /*
         * Very slow when doing directory walks with no regexp.
         */
        char *tmp = dupstr(regexp_in, __FUNCTION__);

        strchopc(tmp, '/');

        snprintf(regexp, sizeof(regexp), "^%s$", tmp);

        myfree(tmp);
#else
        char *tmp = dupstr(regexp_in, __FUNCTION__);

        strchopc(tmp, '/');

        snprintf(regexp, sizeof(regexp), "%s", tmp);

        myfree(tmp);

        is_regexp = false;
#endif
    }

    name = dupstr(name_in, __FUNCTION__);

    strchopc(name, '/');

    /*
     * Get the first tuple of the name.
     */
    top_level_name = dupstr(name, __FUNCTION__);

    for (c = 0; c < strlen(top_level_name); c++) {
        if (top_level_name[c] == '/') {
            top_level_name[c] = 0;
        }
    }

    if (!is_regexp || !*regexp) {
        if (strstr(regexp, "/")) {
            match = (strcasecmp(regexp, name) == 0);
        } else {
            match = (strcasecmp(regexp, top_level_name) == 0);
        }
    } else {
        if (strstr(regexp, "/")) {
            /*
             * Full path match.
             */
            match = regexp_match(regexp, name);
        } else {
            /*
             * Filename or top level dir match only.
             */
            match = regexp_match(regexp, top_level_name);
        }
    }

    myfree(name);
    myfree(top_level_name);

    return (match);
}

void regexp_test (void)
{
    const char *test[] = {
        "foo.c",
        "dir1/foo.c",
        "dir1/FOO.c",
        "dir1/dir2/foo.c",
        "dir1/dir2/fud.c",
        "dir1/dir2//dir3/foo.c",
        "dir1",
        "dir1/dir2",
        "dir1/dir2/",
    };

    boolean is_dir[] = {
        true,
        false,
        false,
        false,
        true,
        true,
        true,
        true,
    };

    const char *regexp[] = {
        "*.c",
        "*.h",
        "dir1/dir2",
        "dir1/dir2/",
        "dir1/dir2/foo.c",
        "dir1/dir2/f*.c",
        "dir1/dir2/*.c",
        "dir1/*/*.c",
        "*/*/*.c",
        "*/*.c",
        "",
        "dir1",
        "dir1/",
    };

    uint32_t i;
    uint32_t j;

    for (j = 0; j < ARRAY_SIZE(regexp); j++) {
        OUT("----------------------------------------------------------");
        for (i = 0; i < ARRAY_SIZE(test); i++) {

            const char *reg = regexp[j];

            if (file_match(reg, test[i], is_dir[i])) {
                OUT("[X] %-20s %-20s", regexp[j], test[i]);
            } else {
                OUT("    %-20s %-20s", regexp[j], test[i]);
            }
        }
    }
}

/*
 * Get rid of dedundant unixy stuff in the filename.
 */
char *filename_cleanup (const char *in_)
{
    char *in = dupstr(in_, __FUNCTION__);

    for (;;) {
        if (strstr(in, "//")) {
            char *newname = strsub(in, "//", "/");
            myfree(in);
            in = newname;
            continue;
        }

        if (strstr(in, "../")) {
            char *newname = strsub(in, "../", "/");
            myfree(in);
            in = newname;
            continue;
        }

        if (strstr(in, "./")) {
            char *newname = strsub(in, "./", "/");
            myfree(in);
            in = newname;
            continue;
        }

        if (strstr(in, "~/")) {
            char *newname = strsub(in, "~/", "/");
            myfree(in);
            in = newname;
            continue;
        }

        break;
    }

    return (in);
}

/*
 * mybasename
 *
 * A safe wrapper for basename to avoid modifications to the input string.
 */
char *mybasename (const char *in, const char *who)
{
    char *tmp = dupstr(in, who);
    char *tmp2 = dupstr(basename(tmp), who);
    myfree(tmp);

    return (tmp2);
}
