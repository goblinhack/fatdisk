/*
 * Copyright (C) 2013 Neil McGill
 *
 * See the LICENSE file.
 */

#ifndef CONFIG_H_INCLUDED
#error include config
#endif

#include <stdarg.h>
#include <stdint.h>

/*
 * Types
 */
#define boolean              char

#ifndef __int8_t_defined
#ifndef _INT8_T
#define _INT8_T
typedef signed char          int8_t;
#endif /*_INT8_T */

#ifndef _INT16_T
#define _INT16_T
typedef short                int16_t;
#endif /* _INT16_T */

#ifndef _INT32_T
#define _INT32_T
typedef int                  int32_t;
#endif /* _INT32_T */

#ifndef _INT64_T
#define _INT64_T
typedef long long            int64_t;
#endif /* _INT64_T */

#ifndef _UINT8_T
#define _UINT8_T
typedef unsigned char        uint8_t;
#endif /*_UINT8_T */

#ifndef _UINT16_T
#define _UINT16_T
typedef unsigned short       uint16_t;
#endif /* _UINT16_T */

#ifndef _UINT32_T
#define _UINT32_T
typedef unsigned int         uint32_t;
#endif /* _UINT32_T */

#ifndef _UINT64_T
#define _UINT64_T
typedef unsigned long int    uint64_t;
#endif /* _UINT64_T */
#endif

#ifndef true
#define true                            1
#endif

#ifndef false
#define false                           0
#endif

#ifndef max
#define max(a,b)                        (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)                        (((a) < (b)) ? (a) : (b))
#endif

/*
 * Simple array routines
 */
#define ARRAY_SIZE(_array_)             (sizeof(_array_)/sizeof(_array_[0]))

/*
 * log.c
 */
#define DIE(args...)                                                          \
    DYING("Died at %s:%s():%u", __FILE__, __FUNCTION__, __LINE__);            \
    CROAK(args);                                                              \
    exit(1);

#ifdef ENABLE_ASSERT
#define ASSERT(x)                                                             \
    if (!(x)) {                                                               \
        DIE("Failed assert");                                                 \
    }
#else
#define ASSERT(x)
#endif

void CROAK(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void DYING(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void OUT(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void VER(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void DBG(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void DBG2(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void DBG3(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void DBG4(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void DBG5(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void WARN(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void ERR(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

/*
 * util.c
 */
void *myzalloc_(uint32_t size, const char *what, const char *func,
                const char *file, const uint32_t line);

void *mymalloc_(uint32_t size, const char *what, const char *func,
                const char *file, const uint32_t line);

void *myrealloc_(void *ptr, uint32_t size, const char *what, const char *func,
                 const char *file, const uint32_t line);

void myfree_(void *ptr, const char *func, const char *file,
             const uint32_t line);

char *dupstr_(const char *in, const char *what, const char *func,
              const char *file, const uint32_t line);

/*
 * file.c
 */
unsigned char *file_read(const char *filename, int64_t *len);
unsigned char *file_read_from(const char *filename, int64_t offset,
                              int64_t amount);
int64_t file_write(const char *filename, unsigned char *buffer, int64_t len);
int64_t file_write_at(const char *filename, int64_t offset,
                      unsigned char *buffer, int64_t len);
boolean file_exists(const char *filename);
boolean dir_exists(const char *dirname);
unsigned char *file_read_if_exists(const char *filename, int64_t *out_len);
int64_t file_size(const char *filename);
boolean file_is_special(const char *filename);
boolean file_non_zero_size_exists(const char *filename);
boolean file_mtime(const char *filename,
                   int32_t *day,
                   int32_t *month,
                   int32_t *year);
uint32_t getumask(void);
boolean file_match(const char *regexp_in, const char *name_in, boolean is_dir);
char *filename_cleanup(const char *in_);
char *mybasename(const char *in, const char *who);

/*
 * dir.c
 */
#include "tree.h"

typedef struct tree_file_node_ {
    tree_key_string tree;
    int8_t is_file:1;
} tree_file_node;

boolean dir_exists(const char *filename);

tree_root *dirlist(const char *dir,
                   const char *include_suffix,
                   const char *exclude_suffix,
                   boolean include_dirs);
tree_root *dirlist_recurse(const char *dir,
                           const char *include_suffix,
                           const char *exclude_suffix,
                           boolean include_dirs);

void dirlist_free(tree_root **root);
char *dir_dot(void);
char *dir_dotdot(char *in);
char *dospath2unix(char *in);
boolean mkpath(const char *path, uint32_t mode);

/*
 * string.c
 */

/*
 * msys functions seem to accept either / or \ so we don't need to worry.
 */
#define DSEP "/"
#define DCHAR '/'

char *substr(const char *in, int32_t pos, int32_t len);
char *strappend(const char *in, const char *append);
char *strprepend(const char *in, const char *prepend);
char *strsub(const char *in, const char *remove, const char *replace_with);
void strchop(char *s);
void strchopc(char *s, char c);
int32_t strisregexp(const char *in);
void strnoescape(char *uncompressed);
char *dynprintf(const char *fmt, ...);
char *dynvprintf(const char *fmt, va_list args);
boolean hex_dump(void *addr, uint64_t offset, uint64_t len);
boolean cat(void *addr, uint64_t offset, uint64_t len);
boolean regexp_match(const char *reg, const char *find);
void regexp_test(void);

/*
 * backtrace.c
 */
void backtrace_print(void);

struct traceback_;
typedef struct traceback_ * tracebackp;

tracebackp traceback_alloc(void);
void traceback_free(tracebackp);
void traceback_stdout(tracebackp);
void traceback_stderr(tracebackp);

/*
 * util.c
 */
void *myzalloc_(uint32_t size, const char *what, const char *func,
                const char *file, const uint32_t line);

void *mymalloc_(uint32_t size, const char *what, const char *func,
                const char *file, const uint32_t line);

void *myrealloc_(void *ptr, uint32_t size, const char *what, const char *func,
                 const char *file, const uint32_t line);


void myfree_(void *ptr, const char *func, const char *file,
             const uint32_t line);

char *dupstr_(const char *in, const char *what, const char *func,
              const char *file, const uint32_t line);

char *duplstr_(const char *in, const char *what, const char *func,
               const char *file, const uint32_t line);

#include "ptrcheck.h"

/*
 * main.c
 */
void quit(void);
void die(void);

extern boolean opt_verbose;
extern boolean opt_quiet;
extern boolean opt_debug5;
extern boolean opt_debug4;
extern boolean opt_debug3;
extern boolean opt_debug2;
extern boolean opt_debug;
extern boolean croaked;
extern uint32_t opt_sector_size;
extern uint32_t opt_sectors_per_cluster;
