/*
 * Copyright (C) 2013 Neil McGill
 *
 * See the LICENSE file for license.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>

#include "main.h"

static char buf[MAX_STR];
boolean croaked;

static void out_ (const char *fmt, va_list args)
{
    uint32_t len;

    buf[0] = '\0';
    len = (uint32_t)strlen(buf);
    vsnprintf(buf + len, sizeof(buf) - len, fmt, args);

    puts(buf);
    fflush(stdout);
}

void OUT (const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    out_(fmt, args);
    va_end(args);
}

static void verbose_ (const char *fmt, va_list args)
{
    uint32_t len;

    buf[0] = '\0';
    len = (uint32_t)strlen(buf);
    vsnprintf(buf + len, sizeof(buf) - len, fmt, args);

    puts(buf);
    fflush(stdout);
}

void VER (const char *fmt, ...)
{
    va_list args;

    if (!opt_verbose) {
        return;
    }

    va_start(args, fmt);
    verbose_(fmt, args);
    va_end(args);
}

static void dbg_ (const char *fmt, va_list args)
{
    uint32_t len;

    buf[0] = '\0';
    len = (uint32_t)strlen(buf);
    vsnprintf(buf + len, sizeof(buf) - len, fmt, args);

    puts(buf);
    fflush(stdout);
}

void DBG (const char *fmt, ...)
{
    va_list args;

    if (!opt_debug) {
        return;
    }

    va_start(args, fmt);
    dbg_(fmt, args);
    va_end(args);
}

static void dbg2_ (const char *fmt, va_list args)
{
    uint32_t len;

    buf[0] = '\0';
    len = (uint32_t)strlen(buf);
    vsnprintf(buf + len, sizeof(buf) - len, fmt, args);

    puts(buf);
    fflush(stdout);
}

void DBG2 (const char *fmt, ...)
{
    va_list args;

    if (opt_debug2) {
        /*
         * Debug.
         */
    } else {
        return;
    }

    va_start(args, fmt);
    dbg2_(fmt, args);
    va_end(args);
}

static void dbg3_ (const char *fmt, va_list args)
{
    uint32_t len;

    buf[0] = '\0';
    len = (uint32_t)strlen(buf);
    vsnprintf(buf + len, sizeof(buf) - len, fmt, args);

    puts(buf);
    fflush(stdout);
}

void DBG3 (const char *fmt, ...)
{
    va_list args;

    if (opt_debug3) {
        /*
         * Debug.
         */
    } else {
        return;
    }

    va_start(args, fmt);
    dbg3_(fmt, args);
    va_end(args);
}

static void dbg4_ (const char *fmt, va_list args)
{
    uint32_t len;

    buf[0] = '\0';
    len = (uint32_t)strlen(buf);
    vsnprintf(buf + len, sizeof(buf) - len, fmt, args);

    puts(buf);
    fflush(stdout);
}

void DBG4 (const char *fmt, ...)
{
    va_list args;

    if (opt_debug4) {
        /*
         * Debug.
         */
    } else {
        return;
    }

    va_start(args, fmt);
    dbg4_(fmt, args);
    va_end(args);
}

static void warn_ (const char *fmt, va_list args)
{
    uint32_t len;

    buf[0] = '\0';
    len = (uint32_t)strlen(buf);
    vsnprintf(buf + len, sizeof(buf) - len, fmt, args);

    puts(buf);
    fflush(stdout);
}

void WARN (const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    warn_(fmt, args);
    va_end(args);
}

static void dying_ (const char *fmt, va_list args)
{
    uint32_t len;

    buf[0] = '\0';
    len = (uint32_t)strlen(buf);

    snprintf(buf + len, sizeof(buf) - len, "DYING: ");

    len = (uint32_t)strlen(buf);
    vsnprintf(buf + len, sizeof(buf) - len, fmt, args);

    puts(buf);
    fflush(stdout);
}

static void err_ (const char *fmt, va_list args)
{
    uint32_t len;

    buf[0] = '\0';
    len = (uint32_t)strlen(buf);

    snprintf(buf + len, sizeof(buf) - len, "ERROR: ");

    len = (uint32_t)strlen(buf);
    vsnprintf(buf + len, sizeof(buf) - len, fmt, args);

    fprintf(stderr, "%s\n", buf);
    fflush(stderr);

#ifdef ENABLE_ERR_BACKTRACE
    backtrace_print();
    fflush(stdout);
#endif
}

static void croak_ (const char *fmt, va_list args)
{
    uint32_t len;

    backtrace_print();
    fflush(stdout);

    buf[0] = '\0';
    len = (uint32_t)strlen(buf);

    snprintf(buf + len, sizeof(buf) - len, "\nFATAL ERROR: ");

    len = (uint32_t)strlen(buf);
    vsnprintf(buf + len, sizeof(buf) - len, fmt, args);

    puts(buf);
    fflush(stdout);

    if (croaked) {
        return;
    }

    croaked = true;

    die();
}

void DYING (const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    dying_(fmt, args);
    va_end(args);
}

void ERR (const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    err_(fmt, args);
    va_end(args);
}

void CROAK (const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    croak_(fmt, args);
    va_end(args);

    quit();
}
