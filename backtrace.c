/*
 * Copyright (C) 2013 Neil McGill
 *
 * See the LICENSE file for license.
 */

#include "config.h"

#ifndef WIN32
#include <execinfo.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

#define MAX_TRACEBACK 16

typedef struct traceback_ {
    void *array[MAX_TRACEBACK];
    uint32_t size;
} traceback;

/*
 * backtrace_print
 *
 * Dump a decoded traceback.
 */
void backtrace_print (void)
{
    if (!opt_verbose) {
        return;
    }

#ifndef WIN32
    void *array[10];
    int32_t size;
    char **strings;
    int32_t i;

    size = backtrace(array, ARRAY_SIZE(array));
    strings = backtrace_symbols(array, size);

    for (i = 0; i < size; i++) {
        printf("%s\n", strings[i]);
    }

    free(strings);
#endif
}

/*
 * traceback_alloc
 *
 * Allocate a new traceback.
 */
traceback *traceback_alloc (void)
{
#ifndef WIN32
    traceback *tb;

    tb = (typeof(tb)) malloc(sizeof(*tb));

    tb->size = backtrace(tb->array, MAX_TRACEBACK);

    return (tb);
#else
    return (0);
#endif
}

/*
 * traceback_free
 *
 * Free a traceback.
 */
void traceback_free (traceback *tb)
{
#ifndef WIN32
    free(tb);
#endif
}

/*
 * traceback_stdout
 *
 * Traceback to stdout.
 */
void traceback_stdout (traceback *tb)
{
#ifndef WIN32
    uint32_t i;
    char **strings;

    strings = backtrace_symbols(tb->array, tb->size);

    for (i = 0; i < tb->size; i++) {
        printf("%s\n", strings[i]);

        if (strstr(strings[i], "main +")) {
            break;
        }
    }

    free(strings);
#endif
}

/*
 * traceback.
 *
 * Traceback to stderr.
 */
void traceback_stderr (traceback *tb)
{
#ifndef WIN32
    uint32_t i;
    char **strings;

    strings = backtrace_symbols(tb->array, tb->size);

    for (i = 0; i < tb->size; i++) {
        fprintf(stderr, "%s\n", strings[i]);

        if (strstr(strings[i], "main +")) {
            break;
        }
    }

    free(strings);
#endif
}
