/*
 * Copyright (C) 2013 Neil McGill
 *
 * See the LICENSE file for license.
 *
 * A useful hack to help debug stale/free/bad pointers.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "time.h"

/*
 * How many times we've seen a given pointer and record a traceback at that
 * point.
 */
#define MAX_PER_PTR_HISTORY 3

/*
 * A single event in the life of a pointer.
 */
typedef struct ptrcheck_history_ {
    const char *file;
    const char *func;
    uint32_t    line;
    uint32_t    ms;
    tracebackp tb;
} ptrcheck_history;

/*
 * The life of a single pointer.
 */
typedef struct ptrcheck_ {
    /*
     * For sanity, the pointer itself.
     */
    void *ptr;

    /*
     * The type of memory.
     */
    const char *what;

    /*
     * How much memory it is using.
     */
    uint32_t size;

    /*
     * Where did the pointer get allocated/freed/last_seen from?
     */
    ptrcheck_history allocated_by;
    ptrcheck_history freed_by;
    ptrcheck_history last_seen[MAX_PER_PTR_HISTORY];

    /*
     * Where in the history buffer we are.
     */
    uint16_t last_seen_at;
    uint16_t last_seen_size;

    /*
     * Ignore when looking for leaks.
     */
    uint8_t leak_ignore:1;

} ptrcheck;

/*
 * A hash table so we can check pointers fast.
 */
typedef struct hash_elem_t_ {
    struct hash_elem_t_ *next;
    ptrcheck *context;
} hash_elem_t;

typedef struct hash_t_ {
    uint32_t hash_size;
    hash_elem_t **elements;
} hash_t;

static hash_t *hash;

/*
 * Ring buffer, used for pointer history.
 */
static ptrcheck *ringbuf_next;
static ptrcheck *ringbuf_base;

/*
 * How many old/freed pointers do we keep track of. We use this when we find
 * an unknown pointer to find when it last lived.
 */
static uint32_t ringbuf_max_size = 10000;
static uint32_t ringbuf_current_size;

/*
 * Prototypes.
 */
static void ptrcheck_usage_cleanup(void);

static boolean ptrcheck_init_done;

boolean ptrcheck_init (void)
{
    ptrcheck_init_done = true;

    return (true);
}

void ptrcheck_fini (void)
{
    /*
     * Print memory leaks.
     */
    ptrcheck_usage_print();

    ptrcheck_leak_print();

    ptrcheck_usage_cleanup();
}

/*
 * local_zalloc
 *
 * Wrapper for calloc.
 */
static void *local_zalloc (uint32_t size)
{
    void *p;

    p = calloc(1, size);
    if (!p) {
        DIE("calloc");
    }

    return (p);
}

/*
 * local_free
 *
 * Wrapper for free.
 */
static void local_free (void *ptr)
{
    free(ptr);
}

/*
 * ptr2hash
 *
 * Map a pointer to a hash slot.
 */
static hash_elem_t ** ptr2hash (hash_t *hash_table, void *ptr)
{
    uint32_t slot;

    /*
     * Knock lower 2 bits off of pointer - these are always 0.
     */
    slot = (uint32_t)((((uintptr_t)(ptr)) >> 2) % hash_table->hash_size);

    return (&hash_table->elements[slot]);
}

/*
 * hash_init
 *
 * Create a hash table for all pointers.
 */
static hash_t *hash_init (uint32_t hash_size)
{
    hash_t *hash_table;

    hash_table =
        (typeof(hash_table))
            local_zalloc(sizeof(hash_t));

    hash_table->hash_size = hash_size;

    hash_table->elements =
        (typeof(hash_table->elements))
                local_zalloc(hash_size * sizeof(hash_elem_t *));

    return (hash_table);
}

/*
 * hash_add
 *
 * Store a pointer in our hash.
 */
static void hash_add (hash_t *hash_table, ptrcheck *context)
{
    hash_elem_t **slot;
    hash_elem_t *elem;

    if (!context) {
        return;
    }

    if (!hash_table) {
        return;
    }

    slot = ptr2hash(hash_table, context->ptr);
    elem = *slot;
    while (elem && (elem->context->ptr != context->ptr)) {
        elem = elem->next;
    }

    if (elem != 0) {
        local_free(context);
        return;
    }

    elem = (typeof(elem)) local_zalloc(sizeof(*elem));
    elem->context = context;
    elem->next = *slot;
    *slot = elem;
}

/*
 * hash_find
 *
 * Find a pointer in our hash.
 */
static hash_elem_t *hash_find (hash_t *hash_table, void *ptr)
{
    hash_elem_t **slot;
    hash_elem_t *elem;

    if (!hash_table) {
        return (0);
    }

    if (!ptr) {
        return (0);
    }

    slot = ptr2hash(hash_table, ptr);
    elem = *slot;
    while (elem && (elem->context->ptr != ptr)) {
        elem = elem->next;
    }

    return (elem);
}

/*
 * hash_free
 *
 * Free a pointer from our hash.
 */
static void hash_free (hash_t *hash_table, void *ptr)
{
    hash_elem_t **slot;
    hash_elem_t *prev;
    hash_elem_t *elem;

    if (!hash_table) {
        return;
    }

    if (!ptr) {
        return;
    }

    slot = ptr2hash(hash_table, ptr);
    elem = *slot;
    prev = 0;

    while (elem && (elem->context->ptr != ptr)) {
        prev = elem;
        elem = elem->next;
    }

    if (!elem) {
        return;
    }

    if (prev) {
        prev->next = elem->next;
    } else {
        *slot = elem->next;
    }

    local_free(elem->context);
    local_free(elem);
}

/*
 * ptrcheck_verify_pointer
 *
 * Check a pointer for validity.
 */
static ptrcheck *ptrcheck_verify_pointer (void *ptr,
                                          const char *func,
                                          const char *file,
                                          const uint32_t line,
                                          boolean dont_store)
{
    static const char *bad_pointer_warning = "**BAD POINTER** ";
    static const char *null_pointer_warning = "**NULL POINTER** ";
    uint32_t ring_ptr_size;
    ptrcheck *context;
    hash_elem_t *e;
    uint32_t i;

    if (!ptr) {
        DIE("%s%p %s:%s():%u", null_pointer_warning, ptr, file, func, line);
        return (0);
    }

    /*
     * Check the robust handle is valid.
     */
    e = hash_find(hash, ptr);
    if (e) {
        context = e->context;

        if (dont_store) {
            return (context);
        }

        /*
         * Add some free information that we know the pointer is safe at this
         * point in time.
         */
        context->last_seen[context->last_seen_at].file = file;
        context->last_seen[context->last_seen_at].func = func;
        context->last_seen[context->last_seen_at].line = line;

        if (context->last_seen[context->last_seen_at].tb) {
            local_free(context->last_seen[context->last_seen_at].tb);
        }

        context->last_seen[context->last_seen_at].tb = traceback_alloc();

        context->last_seen_at++;
        context->last_seen_size++;

        if (context->last_seen_at >= MAX_PER_PTR_HISTORY) {
            context->last_seen_at = 0;
        }

        if (context->last_seen_size >= MAX_PER_PTR_HISTORY) {
            context->last_seen_size = MAX_PER_PTR_HISTORY;
        }

        return (context);
    }

    /*
     * We may be about to crash. Complain!
     */
    ERR("%s%p %s:%s():%u", bad_pointer_warning, ptr, file, func, line);

    /*
     * Check the ring buffer to see if we've seen this pointer before.
     */
    context = ringbuf_next;
    context--;

    if (context < ringbuf_base) {
        context = ringbuf_base + ringbuf_max_size - 1;
    }

    ring_ptr_size = ringbuf_current_size;

    /*
     * Walk back through the ring buffer.
     */
    while (ring_ptr_size) {
        /*
         * Found a match?
         */
        if (context->ptr == ptr) {
            fprintf(stderr, "Allocated \"%s\" (%u bytes) at %s:%s():%u\n",
                    context->what,
                    context->size,
                    context->allocated_by.file,
                    context->allocated_by.func,
                    context->allocated_by.line);

            traceback_stderr(context->allocated_by.tb);

            fprintf(stderr, "Freed at %s:%s():%u\n",
                    context->freed_by.file,
                    context->freed_by.func,
                    context->freed_by.line);

            traceback_stderr(context->freed_by.tb);

            /*
             * Dump the pointer history.
             */
            ptrcheck_history *history;

            history = &context->last_seen[context->last_seen_at];

            for (i=0; i < context->last_seen_size; i++) {
                if (--history < context->last_seen) {
                    history = &context->last_seen[MAX_PER_PTR_HISTORY-1];
                }

                fprintf(stderr, "Last seen at [%u] at %s:%s():%u\n",
                        i,
                        history->file,
                        history->func,
                        history->line);

                traceback_stderr(history->tb);
            }

            /*
             * Memory reuse can cause a lot of false hits, so stop after
             * the first match.
             */
            break;
        }

        ring_ptr_size--;
        context--;

        /*
         * Handle wraps.
         */
        if (context < ringbuf_base) {
            context = ringbuf_base + ringbuf_max_size - 1;
        }
    }

    exit(1);

    return (0);
}

/*
 * ptrcheck_alloc
 *
 * Record this pointer.
 */
void *ptrcheck_alloc (void *ptr,
                      const char *what,
                      const uint32_t size,
                      const char *func,
                      const char *file,
                      const uint32_t line)
{
    ptrcheck *context;

    if (!ptr) {
        DIE("null pointer");
    }

    /*
     * Create a hash table to store pointers.
     */
    if (!hash) {
        /*
         * Create enough space for lots of pointers.
         */
        hash = hash_init(1046527 /* prime */);

        if (!hash) {
            return (ptr);
        }
    }

    /*
     * Missing an earlier free?
     */
    if (hash_find(hash, ptr)) {
        DIE("pointer %p alread exists and attempting to add again", ptr);
        return (ptr);
    }

    /*
     * And a ring buffer to store old pointer into.
     */
    if (!ringbuf_next) {
        ringbuf_next =
            (typeof(ringbuf_next))
                local_zalloc(sizeof(ptrcheck) * ringbuf_max_size);

        ringbuf_base = ringbuf_next;
        ringbuf_current_size = 0;
    }

    /*
     * Allocate a block of data to describe the pointer and owner.
     */
    context =
        (typeof(context))
            local_zalloc(sizeof(ptrcheck));

    /*
     * Populate the data block.
     */
    context->ptr = ptr;
    context->what = what;
    context->size = size;
    context->allocated_by.func = file;
    context->allocated_by.file = func;
    context->allocated_by.line = line;
    context->allocated_by.tb = traceback_alloc();

    /*
     * Add it to the hash. Not the ring buffer (only when freed).
     */
    hash_add(hash, context);

    return (ptr);
}

/*
 * ptrcheck_free
 *
 * Check a pointer is valid and if so add it to the ring buffer. If not,
 * return false and avert the myfree(), just in case.
 */
boolean ptrcheck_free (void *ptr,
                       const char *func,
                       const char *file,
                       const uint32_t line)
{
    ptrcheck *context;

    if (!ptr) {
        DIE("null pointer");

        return (false);;
    }

    context = ptrcheck_verify_pointer(ptr, file, func, line,
                                      true /* dont store */);
    if (!context) {
        return (false);
    }

    /*
     * Add some free information that we know the pointer is safe at this
     * point in time.
     */
    context->freed_by.file = file;
    context->freed_by.func = func;
    context->freed_by.line = line;
    context->freed_by.tb = traceback_alloc();

    /*
     * Add the free info to the ring buffer.
     */
    memcpy(ringbuf_next, context, sizeof(ptrcheck));

    /*
     * Take care of wraps.
     */
    ringbuf_next++;
    if (ringbuf_next >= ringbuf_base + ringbuf_max_size) {
        ringbuf_next = ringbuf_base;
    }

    /*
     * Increment the ring buffer used size up to the limit.
     */
    if (ringbuf_current_size < ringbuf_max_size) {
        ringbuf_current_size++;
    }

    hash_free(hash, ptr);

    return (true);
}

/*
 * ptrcheck_verify
 *
 * Check a pointer for validity.
 */
boolean ptrcheck_verify (void *ptr,
                         const char *func,
                         const char *file,
                         const uint32_t line)
{
    return (ptrcheck_verify_pointer(ptr, file, func, line,
                                    false /* don't store */) != 0);
}

/*
 * ptrcheck_verify
 *
 * Check a pointer for validity with no recording of history.
 */
boolean ptrcheck_fast_verify (void *ptr,
                              const char *func,
                              const char *file,
                              const uint32_t line)
{
    return (ptrcheck_verify_pointer(ptr, file, func, line,
                                    true /* don't store */) != 0);
}

/*
 * ptrcheck_leak_print
 *
 * Free a pointer from our hash.
 */
void ptrcheck_leak_print (void)
{
    hash_elem_t **slot;
    hash_elem_t *elem;
    ptrcheck *context;
    uint32_t i;
    uint32_t j;
    uint32_t leak;

    leak = 0;

    if (!hash) {
        return;
    }

    for (i = 0; i < hash->hash_size; i++) {
        slot = &hash->elements[i];
        elem = *slot;

        while (elem) {
            context = elem->context;

            if (context->leak_ignore) {
                elem = elem->next;
                continue;
            }

            leak++;

            fprintf(stderr, "Leak %p \"%s\" (%u bytes) at %s:%s():%u\n",
                    context->ptr,
                    context->what,
                    context->size,
                    context->allocated_by.file,
                    context->allocated_by.func,
                    context->allocated_by.line);

            traceback_stderr(context->allocated_by.tb);

            /*
             * Dump the pointer history.
             */
            ptrcheck_history *history;

            history = &context->last_seen[context->last_seen_at];

            for (j=0; j < context->last_seen_size; j++) {
                if (--history < context->last_seen) {
                    history = &context->last_seen[MAX_PER_PTR_HISTORY-1];
                }

                fprintf(stderr, "Last seen at [%u] at %s:%s():%u\n",
                        j,
                        history->file,
                        history->func,
                        history->line);

                traceback_stderr(history->tb);
            }

            elem = elem->next;
        }
    }

    if (!leak) {
        fprintf(stderr, "No memory leaks!\n");
    }
}

/*
 * ptrcheck_leak_snapshot
 *
 * Set so we only see leaks since now.
 */
void ptrcheck_leak_snapshot (void)
{
    hash_elem_t **slot;
    hash_elem_t *elem;
    ptrcheck *context;
    uint32_t i;

    if (!hash) {
        return;
    }

    for (i = 0; i < hash->hash_size; i++) {
        slot = &hash->elements[i];
        elem = *slot;

        while (elem) {
            context = elem->context;

            context->leak_ignore = true;

            elem = elem->next;
        }
    }
}

typedef struct tree_usage_node_ {
    tree_key_string tree;
    uint32_t blocks;
    uint32_t total;
} tree_usage_node;

/*
 * ptrcheck_usage_print
 *
 * Print top memory allocators
 */
void ptrcheck_usage_print (void)
{
    hash_elem_t **slot;
    hash_elem_t *elem;
    ptrcheck *context;
    tree_root *root;
    uint32_t i;

    if (!hash) {
        return;
    }

    root = tree_alloc(TREE_KEY_STRING, "TREE ROOT: show mem");

    for (i = 0; i < hash->hash_size; i++) {
        slot = &hash->elements[i];
        elem = *slot;

        while (elem) {
            context = elem->context;

            tree_usage_node find;
            tree_usage_node *node;
            tree_usage_node *target;

            memset(&find, 0, sizeof(find));
            find.tree.key = (char*)context->what;

            target = (typeof(target)) tree_find(root, &find.tree.node);
            if (!target) {
                node = (typeof(node)) myzalloc(sizeof(*node),
                                               "TREE NODE: show mem");
                node->tree.key = (char*)context->what;
                node->total = context->size;
                node->blocks = 1;

                if (!tree_insert(root, &node->tree.node)) {
                    DIE("insert usage %s", context->what);
                }
            } else {
                target->total += context->size;
                target->blocks++;
            }

            elem = elem->next;
        }
    }

    boolean first = true;

    while (tree_root_top(root)) {
        tree_usage_node *largest;
        tree_usage_node *node;

        largest = 0;

        TREE_WALK(root, node) {
            if (!largest) {
                if (first) {
                    first = false;
                    OUT("%-30s %s", "Allocator", "Total memory used");
                    OUT("%-30s %s", "---------", "-----------------");
                }

                largest = node;
                continue;
            }

            if (node->total > largest->total) {
                largest = node;
            }
        }

        if (largest) {
            OUT("%-30s %u [%u blocks]", largest->tree.key, largest->total,
                largest->blocks);
            tree_remove(root, &largest->tree.node);
            myfree(largest);
        }
    }

    tree_destroy(&root, 0);
}

/*
 * ptrcheck_usage_cleanup
 *
 * Print top memory allocators
 */
static void ptrcheck_usage_cleanup (void)
{
    hash_elem_t **slot;
    hash_elem_t *elem;
    hash_elem_t *next;
    ptrcheck *context;
    uint32_t i;
    uint32_t j;

    if (!hash) {
        return;
    }

    for (i = 0; i < hash->hash_size; i++) {
        slot = &hash->elements[i];
        elem = *slot;

        while (elem) {
            context = elem->context;

            if (context->allocated_by.tb) {
                local_free(context->allocated_by.tb);
            }

            if (context->freed_by.tb) {
                local_free(context->freed_by.tb);
            }

            for (j=0; j < MAX_PER_PTR_HISTORY; j++) {
                if (context->last_seen[j].tb) {
                    local_free(context->last_seen[j].tb);
                }
            }

            next = elem->next;
            local_free(elem);
            elem = next;
        }
    }

    local_free(hash->elements);
    local_free(hash);

    hash = 0;
}
