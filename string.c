/*
 * Copyright (C) 2013 Neil McGill
 *
 * See the LICENSE file for license.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <regex.h>
#include <unistd.h>

#include "main.h"
#include "string.h"

/*
 * return a part of a string.
 *
 * substr("foo.zip", -4, 4) -> "zip"
 */
char *substr (const char *in, int32_t pos, int32_t len)
{
    int32_t slen;
    char *out;

    if (!in) {
        return (0);
    }

    slen = (uint32_t)strlen(in);

    /*
     * Negative positions from the end.
     */
    if (pos < 0) {
        pos = slen + pos;
    }

    if (pos < 0) {
        pos = 0;
    }

    if (len < 0) {
        len = 0;
    }

    if (pos > slen) {
        pos = slen;
    }

    if (len > slen - pos) {
        len = slen - pos;
    }

    out = (typeof(out)) mymalloc(len + sizeof((char)'\0'), "substr");
    if (!out) {
        return (0);
    }

    memcpy(out, &(in[pos]), len);
    out[len] = '\0';

    return (out);
}

/*
 * Replace part of a string with another.
 *
 * strsub("foo.zip", ".zip", ""); -> "foo"
 */
char *strsub (const char *in, const char *old, const char *replace_with)
{
    char *buf;
    const char *at;
    int32_t newlen;
    int32_t oldlen;
    int32_t len;

    if (!in || !old || !replace_with) {
        return (0);
    }

    at = strstr(in, old);
    if (!at) {
        buf = dupstr(in, __FUNCTION__);
        return (buf);
    }

    oldlen = (uint32_t)strlen(old);
    newlen = (uint32_t)strlen(replace_with);

    len = (uint32_t)strlen(in) - oldlen + newlen;
    buf = (typeof(buf)) myzalloc(len + sizeof((char)'\0'), "strsub 2");
    if (!buf) {
        return (0);
    }

    *buf = '\0';
    strncpy(buf, in, at - in);
    strcat(buf, replace_with);
    strcat(buf, at + oldlen);

    return (buf);
}

/*
 * Add onto the end of a string.
 *
 * strsub("foo", ".zip"); -> "foo.zip"
 */
char *strappend (const char *in, const char *append)
{
    char *buf;
    int32_t newlen;
    int32_t len;

    if (!in || !append) {
        return (0);
    }

    newlen = (uint32_t)strlen(append);
    len = (uint32_t)strlen(in) + newlen;
    buf = (typeof(buf)) myzalloc(len + sizeof((char)'\0'), "strappend");
    if (!buf) {
        return (0);
    }

    strcpy(buf, in);
    strcat(buf, append);

    return (buf);
}

/*
 * Add onto the start of a string.
 *
 * strsub("foo", "bar"); -> "barfoo"
 */
char *strprepend (const char *in, const char *prepend)
{
    char *buf;
    int32_t newlen;
    int32_t len;

    if (!in || !prepend) {
        return (0);
    }

    newlen = (uint32_t)strlen(prepend);
    len = (uint32_t)strlen(in) + newlen;
    buf = (typeof(buf)) myzalloc(len + sizeof((char)'\0'), "strprepend");
    if (!buf) {
        return (0);
    }

    strcpy(buf, prepend);
    strcat(buf, in);

    return (buf);
}

/*
 * Removes trailing whitespace.
 */
void strchop (char *s)
{
    uint32_t size;
    char *end;

    size = (uint32_t)strlen(s);
    if (!size) {
        return;
    }

    end = s + size - 1;
    while ((end >= s) && (*end == ' ')) {
        end--;
    }

    *(end + 1) = '\0';
}

/*
 * Removes trailing characters.
 */
void strchopc (char *s, char c)
{
    uint32_t size;
    char *end;

    size = (uint32_t)strlen(s);
    if (!size) {
        return;
    }

    end = s + size - 1;
    while ((end >= s) && (*end == c)) {
        end--;
    }

    *(end + 1) = '\0';
}

int32_t strisregexp (const char *in)
{
    const char *a = in;
    char c;

    while ((c = *a++)) {
        switch (c) {
        case '[': return (1);
        case ']': return (1);
        case '{': return (1);
        case '}': return (1);
        case '+': return (1);
        case '$': return (1);
        case '^': return (1);
        case '*': return (1);
        }
    }

    return (false);
}

static const char *dynvprintf_ (const char *fmt, va_list args)
{
    static char buf[MAX_STR];

    buf[0] = '\0';
    vsnprintf(buf, sizeof(buf), fmt, args);

    return (buf);
}

/*
 * dynamically allocate a printf formatted string. e.g.
 * dynprintf("%sbar", "foo"); => "foobar"
 */
char *dynprintf (const char *fmt, ...)
{
    const char *ret;
    va_list args;

    va_start(args, fmt);
    ret = dynvprintf_(fmt, args);
    va_end(args);

    return (dupstr(ret, __FUNCTION__));
}

char *dynvprintf (const char *fmt, va_list args)
{
    static char buf[MAX_STR];

    buf[0] = '\0';
    vsnprintf(buf, sizeof(buf), fmt, args);

    return (dupstr(buf, __FUNCTION__));
}

/*
 * hex_dump
 *
 * Returns false if we finished with an empty block.
 */
boolean hex_dump (void *addr, uint64_t offset, uint64_t len)
{
#define HEX_DUMP_WIDTH 16
    boolean skipping_blanks = false;
    uint8_t empty[HEX_DUMP_WIDTH] = {0};
    uint8_t buf[HEX_DUMP_WIDTH + 1];
    uint8_t *pc = (typeof(pc)) addr;
    uint64_t i;
    uint32_t x;

    if (!len) {
        return (false);
    }

    for (i = 0, x = 0; i < len; i++, x++) {
        if ((i % HEX_DUMP_WIDTH) == 0) {
            if (!skipping_blanks) {
                if (i != 0) {
                    printf(" |%*s|\n", HEX_DUMP_WIDTH, buf);
                }
            }

            /*
             * Skip blank blocks.
             */
            if (!memcmp(pc + i, empty, sizeof(empty))) {
                i += HEX_DUMP_WIDTH - 1;
                skipping_blanks = true;
                buf[0] = '\0';
                continue;
            }

            printf("  %08X ", (uint32_t)(i + offset));

            x = 0;
        }

        if (x && (((i % (HEX_DUMP_WIDTH/2))) == 0)) {
            printf(" ");
        }

        skipping_blanks = false;

        printf(" %02X", pc[i]);

        if ((pc[i] < ' ') || (pc[i] > '~')) {
            buf[i % HEX_DUMP_WIDTH] = '.';
        } else {
            buf[i % HEX_DUMP_WIDTH] = pc[i];
        }

        buf[(i % HEX_DUMP_WIDTH) + 1] = '\0';
    }

    if (!buf[0]) {
        if (skipping_blanks) {
            printf("  *\n");
        }

        return (false);
    }

    while ((i % HEX_DUMP_WIDTH) != 0) {
        printf ("   ");

        if (i && (((i % (HEX_DUMP_WIDTH/2))) == 0)) {
            printf (" ");
        }

        i++;
    }

    printf(" |%*s|\n", -HEX_DUMP_WIDTH, buf);

    return (true);
}

/*
 * cat
 *
 * Dump data to the console
 */
boolean cat (void *addr, uint64_t offset, uint64_t len)
{
    if (!len) {
        return (false);
    }

    if (fwrite(addr, len, 1, stdout) != len) {
        return (false);
    }

    return (true);
}

/*
 * Match a regular expression
 */
boolean regexp_match (const char *reg, const char *find)
{
    char msgbuf[MAX_STR];
    regex_t regex;
    int ret;

    /*
     * This is a LOT faster.
     */
    if (!strisregexp(reg)) {
        return (strcasecmp(reg, find) == 0);
    }

    /*
     * Compile regular expression
     */
    ret = regcomp(&regex, reg, REG_ICASE | REG_NOSUB );
    if (ret) {
        DIE("Could not compile regex [%s]", reg);
    }

    /*
     * Execute regular expression
     */
    ret = regexec(&regex, find, 0, NULL, 0);
    if (!ret) {
        /*
         * Match
         */
        regfree(&regex);
        return (true);
    } else if (ret == REG_NOMATCH) {
        /*
         * No match
         */
        regfree(&regex);
        return (false);
    }

    regerror(ret, &regex, msgbuf, sizeof(msgbuf));
    regfree(&regex);

    DIE("Regex match failed: %s\n", msgbuf);
}
