/*
 * Copyright (C) 2013 Neil McGill
 *
 * See the LICENSE file for license.
 */

#ifndef TREE_H
#define TREE_H

typedef enum { RB_RED, RB_BLACK } node_color;

typedef struct tree_node_ {
    struct tree_node_ *left;
    struct tree_node_ *right;
    struct tree_node_ *parent;
    node_color color;
    uint8_t is_static_mem:1;
} tree_node;

typedef int32_t (*tree_key_func)(const tree_node *, const tree_node *);
typedef boolean (*tree_destroy_func)(tree_node *);

typedef enum {
    TREE_KEY_CUSTOM,
    TREE_KEY_INTEGER,
    TREE_KEY_TWO_INTEGER,
    TREE_KEY_STRING,
    TREE_KEY_POINTER,
} tree_key_type;

typedef struct tree_root_ {
    tree_key_func compare_func;

    tree_node *node;

    /*
     * Negative offset to start of base structure.
     */
    uint16_t offset;

    /*
     * TREE_KEY_CUSTOM, etc...
     */
    tree_key_type type;
} tree_root;

typedef struct {
    tree_node node;
    char *key;
} tree_key_string;

typedef struct {
    tree_node node;
    void *key;
} tree_key_pointer;

typedef struct {
    tree_node node;
    int32_t key;
} tree_key_int;

typedef struct {
    tree_node node;
    int32_t key1;
    int32_t key2;
} tree_key_two_int;

tree_root *tree_alloc(tree_key_type, const char *name);
tree_root *tree_alloc_custom(tree_key_func, const char *name);
tree_node *tree_root_top(tree_root *);
tree_node *tree_top(tree_node *);
tree_node *tree_root_last(tree_root *);
tree_node *tree_root_first(tree_root *);
tree_node *tree_first(tree_node *);
tree_node *tree_root_get_nth(tree_root *, uint32_t n);
tree_node *tree_root_get_random(tree_root *);
tree_node *tree_last(tree_node *);
tree_node *tree_get_next(tree_root *, tree_node *top, tree_node *);
tree_node *tree_get_prev(tree_root *, tree_node *top, tree_node *);
uint32_t tree_root_size(tree_root *);
uint32_t tree_size(const tree_node *top);
tree_node *tree_find(tree_root *, const tree_node *);

typedef boolean (*tree_walker_func)(const tree_node *, void *arg);

tree_node *tree_next(tree_root *root, tree_node *node);
tree_node *tree_prev(tree_root *root, tree_node *node);
boolean tree_walk(tree_root *, tree_walker_func, void *arg);
boolean tree_walk_reverse(tree_root *, tree_walker_func, void *arg);

boolean tree_insert(tree_root *, tree_node *);
boolean tree_insert_static(tree_root *, tree_node *);
boolean tree_remove(tree_root *, tree_node *);
boolean tree_remove_found_node(tree_root *, tree_node *);
boolean tree_root_is_empty(tree_root *root);
void tree_empty(tree_root *, tree_destroy_func);
void tree_destroy(tree_root **, tree_destroy_func);

/*
 * Convert a tree node back into the base structure that contains it.
 */
static inline void *retnode2ctx (tree_root *root, tree_node *node)
{
    if (!node) {
        return (0);
    }

    return ((void*) (((char*)node) - root->offset));
}

static inline void *getnode2ctx (tree_root *root, tree_node **out,
                                 tree_node *node)
{
    if (!node) {
        return (0);
    }

    *out = (tree_node*) ((void*) (((char*)node) - root->offset));

    return (node);
}

#define TREE_WALK(ROOT, NODE)                                               \
    typeof((NODE)->tree) saved_key;                                         \
    extern int32_t gcc_no_warn;                                             \
                                                                            \
    if (ROOT)                                                               \
                                                                            \
    for ((NODE) = (typeof(NODE)) tree_first((ROOT)->node);                  \
                                                                            \
        gcc_no_warn =                                                       \
            (NODE) ? saved_key = (NODE)->tree, 0 : 0, (NODE);               \
                                                                            \
        (NODE) = (typeof(NODE))                                             \
            tree_get_next((ROOT), (ROOT)->node, &saved_key.node))


#define TREE_WALK_REVERSE(ROOT, NODE)                                       \
    typeof((NODE)->tree) saved_key;                                         \
    extern int32_t gcc_no_warn;                                             \
                                                                            \
    if (ROOT)                                                               \
                                                                            \
    for ((NODE) = (typeof(NODE)) tree_last((ROOT)->node);                   \
                                                                            \
        gcc_no_warn =                                                       \
            (NODE) ? saved_key = (NODE)->tree, 0 : 0, (NODE);               \
                                                                            \
        (NODE) = (typeof(NODE))                                             \
            tree_get_prev((ROOT), (ROOT)->node, &saved_key.node))

#define TREE_WALK_UNSAFE(ROOT, NODE)                                        \
    typeof(NODE) NEXT;                                                      \
                                                                            \
    for (NEXT = (NODE) = (typeof(NODE)) tree_root_first((ROOT));            \
         (NODE = (NEXT)) != 0;                                              \
         NEXT = (typeof(NODE)) tree_next((ROOT), (tree_node*)(NODE)))       \

#define TREE_WALK_REVERSE_UNSAFE(ROOT, NODE)                                \
    typeof(NODE) PREV;                                                      \
                                                                            \
    for (PREV = (NODE) = (typeof(NODE)) tree_root_last((ROOT));             \
         (NODE = (PREV)) != 0;                                              \
         PREV = (typeof(NODE)) tree_prev((ROOT), (tree_node*)(NODE)))       \

#define TREE2_WALK_UNSAFE(ROOT, OUT)                                       \
    typeof(OUT) NEXT;                                                      \
    typeof(OUT) NODE;                                                      \
                                                                           \
    for (NEXT = (NODE) = (typeof(NODE))                                    \
         getnode2ctx((ROOT), (tree_node**) &(OUT), tree_root_first((ROOT))); \
         (NODE = (NEXT)) != 0;                                             \
         NEXT = (typeof(NODE))                                             \
         getnode2ctx((ROOT), (tree_node**) &(OUT),                         \
                     tree_next((ROOT), (tree_node*)(NODE))))               \
                                                                            
#define TREE2_WALK_REVERSE_UNSAFE(ROOT, OUT)                               \
    typeof(OUT) PREV;                                                      \
    typeof(OUT) NODE;                                                      \
                                                                           \
    for (PREV = (NODE) = (typeof(NODE))                                    \
         getnode2ctx((ROOT), (tree_node**) &(OUT), tree_root_last((ROOT))); \
         (NODE = (PREV)) != 0;                                             \
         PREV = (typeof(NODE))                                             \
         getnode2ctx((ROOT), (tree_node**) &(OUT),                         \
                     tree_prev((ROOT), (tree_node*)(NODE))))               \

#define TREE2_WALK(ROOT, NODE)                                              \
    typeof((NODE)->tree2) saved_key;                                        \
    extern int32_t gcc_no_warn;                                             \
                                                                            \
    if (ROOT)                                                               \
                                                                            \
    for ((NODE) = (typeof(NODE))                                            \
         retnode2ctx(ROOT, tree_first((ROOT)->node));                       \
                                                                            \
        gcc_no_warn =                                                       \
            (NODE) ? saved_key = (NODE)->tree2, 0 : 0, (NODE);              \
                                                                            \
        (NODE) = (typeof(NODE))                                             \
            retnode2ctx(ROOT,                                               \
                tree_get_next((ROOT), (ROOT)->node, &saved_key.node)))

#endif
