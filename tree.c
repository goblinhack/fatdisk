/*
 * Copyright (C) 2013 Neil McGill
 *
 * See the LICENSE file for license.
 *
 * A red/black tree implementation. Rules:
 *
 * 1. Each node is either red or black.
 * 2. The root node is black.
 * 3. All leaf nodes are black.
 * 4. Every red node has two black children.
 * 5. Any paths from node to leaf has the same number of black nodes.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "main.h"

int32_t gcc_no_warn;

static inline int32_t
tree_key_int32_compare_func (const tree_node *a, const tree_node *b)
{
    tree_key_int *A = (typeof(A))a;
    tree_key_int *B = (typeof(B))b;

    if (A->key < B->key) {
        return (-1);
    }

    if (A->key > B->key) {
        return (1);
    }

    return (0);
}

static int32_t
tree_key_two_int32_compare_func (const tree_node *a, const tree_node *b)
{
    tree_key_two_int *A = (typeof(A))a;
    tree_key_two_int *B = (typeof(B))b;

    if (A->key1 < B->key1) {
        return (-1);
    }

    if (A->key1 > B->key1) {
        return (1);
    }

    if (A->key2 < B->key2) {
        return (-1);
    }

    if (A->key2 > B->key2) {
        return (1);
    }

    return (0);
}

static int32_t tree_key_string_compare_func (const tree_node *a,
                                             const tree_node *b)
{
    tree_key_string *A = (typeof(A))a;
    tree_key_string *B = (typeof(B))b;

    return (strcmp(A->key, B->key));
}

static int32_t tree_key_pointer_compare_func (const tree_node *a,
                                              const tree_node *b)
{
    tree_key_pointer *A = (typeof(A))a;
    tree_key_pointer *B = (typeof(B))b;

    if (A->key < B->key) {
        return (-1);
    }

    if (A->key > B->key) {
        return (1);
    }

    return (0);
}

static inline int32_t tree_node_compare_func (const tree_root *root,
                                              const tree_node *a,
                                              const tree_node *b)
{
    switch (root->type) {
    case TREE_KEY_CUSTOM:
        return (*(root->compare_func))(a, b);
    case TREE_KEY_INTEGER:
        return (tree_key_int32_compare_func(a, b));
    case TREE_KEY_TWO_INTEGER:
        return (tree_key_two_int32_compare_func(a, b));
    case TREE_KEY_STRING:
        return (tree_key_string_compare_func(a, b));
    case TREE_KEY_POINTER:
        return (tree_key_pointer_compare_func(a, b));
    }

    return (0);
}

tree_node *tree_root_top (tree_root *root)
{
    if (!root) {
        return (0);
    }

    return (root->node);
}

tree_node *tree_top (tree_node *node)
{
    while (node) {
        if (!node->parent) {
            return (node);
        }
    }

    return (node);
}

static tree_node *parent (const tree_node *node)
{
    ASSERT(node);

    return (node->parent);
}

static tree_node *grandparent (const tree_node *node)
{
    ASSERT(node);
    ASSERT(node->parent);

    return (node->parent->parent);
}

static tree_node *sibling (const tree_node *node)
{
    ASSERT(node);
    ASSERT(node->parent);

    if (node == node->parent->left) {
        return (node->parent->right);
    } else {
        return (node->parent->left);
    }
}

static tree_node *uncle (const tree_node * node)
{
    ASSERT(node);
    ASSERT(node->parent);

    return (sibling(node->parent));
}

/*
 * Leaf nodes that are nil and the nil root node are black.
 */
static node_color tree_node_color (const tree_node * node)
{
    return (node ? node->color : RB_BLACK);
}

/*
 * Tree has no contents?
 */
boolean tree_root_is_empty (tree_root *root)
{
    tree_node *top;

    if (!root) {
        return (true);
    }

    top = root->node;
    if (!top) {
        return (true);
    }

    return (false);
}

/*
 * Get the least node in the subtree.
 */
tree_node *tree_root_first (tree_root *root)
{
    tree_node *top;

    if (!root) {
        return (0);
    }

    top = root->node;
    if (!top) {
        return (0);
    }

    while (top->left) {
        top = top->left;
    }

    return (top);
}

/*
 * Get the highest node in the subtree.
 */
tree_node *tree_root_last (tree_root *root)
{
    tree_node *top;

    if (!root) {
        return (0);
    }

    top = root->node;
    if (!top) {
        return (0);
    }

    while (top->right) {
        top = top->right;
    }

    return (top);
}

/*
 * Get the least node in the subtree.
 */
tree_node *tree_first (tree_node *top)
{
    if (!top) {
        return (0);
    }

    while (top->left) {
        top = top->left;
    }

    return (top);
}

/*
 * Get the highest node in the subtree.
 */
tree_node *tree_last (tree_node *top)
{
    if (!top) {
        return (0);
    }

    while (top->right) {
        top = top->right;
    }

    return (top);
}

/*
 * Find the next highest node.
 */
tree_node *tree_get_next (tree_root *root,
                          tree_node *top,
                          tree_node *node)
{
    tree_node *subtree;
    int32_t compare;

    if (!top) {
	return (0);
    }

    compare = tree_node_compare_func(root, top, node);

    if (compare == 0) {
        /*
         * top == node
         *
         * Dive into the right tree and return the least node.
         *
         *        4   (top 4, node 4, look at 7)
         *       / \
         *      3   8
         *     /   / \
         *    1   7   9
         */
        if (!top->right) {
            return (0);
        }

	return (tree_first(top->right));
    }

    if (compare < 0) {
        /*
         * top < node
         *
         * Dive into the right tree.
         *
         *        4   (top 4, node 5, look at 8)
         *       / \
         *      3   8
         *     /   / \
         *    1   7   9
         */
	return (tree_get_next(root, top->right, node));
    }

    /*
     * top > node
     *
     * Dive into the left tree.
     *
     *        4   (top 4, node 3, look at 1)
     *       / \
     *      3   8
     *     /   / \
     *    1   7   9
     */
    subtree = tree_get_next(root, top->left, node);
    if (subtree) {
        return (subtree);
    }

    /*
     * top > node
     *
     * If there is no subtree.
     *
     *        4   (top 1, node 0, look at 1)
     *       / \
     *      3   8
     *     /   / \
     *    1   7   9
     */
    return (top);
}

/*
 * Find the next lowest node.
 */
tree_node *tree_get_prev (tree_root *root,
                          tree_node *top,
                          tree_node *node)
{
    tree_node *subtree;
    int32_t compare;

    if (!top) {
	return (0);
    }

    compare = tree_node_compare_func(root, top, node);

    if (compare == 0) {
        /*
         * top == node
         *
         * Dive into the left tree and return the greatest node.
         *
         *        4   (top 4, node 4, look at 3)
         *       / \
         *      3   8
         *     /   / \
         *    1   7   9
         */
        if (!top->left) {
            return (0);
        }

        return (tree_last(top->left));
    }

    if (compare < 0) {
        /*
         * top < node
         *
         * Dive into the right tree.
         *
         *        4   (top 4, node 5, look at 8)
         *       / \
         *      3   8
         *     /   / \
         *    1   7   9
         */
        subtree = tree_get_prev(root, top->right, node);
        if (subtree) {
            return (subtree);
        }

        /*
         * top > node
         *
         * If there is no subtree.
         *
         *        4   (top 4, node 5, return 4)
         *       /
         *      3
         *     /
         *    1
         */
        return (top);
    }

    /*
     * top > node
     *
     * Dive into the left tree.
     *
     *        4   (top 4, node 3, look at 1)
     *       / \
     *      3   8
     *     /   / \
     *    1   7   9
     */
    return (tree_get_prev(root, top->left, node));
}

/*
 * How many nodes.
 */
uint32_t tree_size (const tree_node *top)
{
    if (!top) {
        return (0);
    }

    return (1 + tree_size(top->left) + tree_size(top->right));
}

/*
 * Find a node.
 */
tree_node *tree_find (tree_root *root, const tree_node *target)
{
    tree_node *top;
    int32_t compare;

    if (!root) {
        return (0);
    }

    top = root->node;
    while (top) {
	compare = tree_node_compare_func(root, top, target);
        if (compare == 0) {
	    return (top);
        }

        if (compare < 0) {
	    top = top->right;
            continue;
        }

        top = top->left;
    }

    return (0);
}

#ifdef OLD_TREE_WALK_MUCH_SLOWER
/*
 * Walk all nodes. Not safe if next node is destroyed. Use TREE_WALK instead.
 */
int tree_walk (tree_root *root, tree_walker_func walker_func, void *arg)
{
    tree_node *top;
    tree_node *node;
    tree_node *next;

    top = root->node;
    if (!top) {
        return (1);
    }

    node = tree_first(top);

    while (node) {
        next = tree_get_next(root, top, node);

        if (!(*walker_func)(node, arg)) {
            return (0);
        }

        node = next;
    }

    return (1);
}
#endif

/*
 * Walk all nodes. Not safe if next node is destroyed. Use TREE_WALK instead.
 */
tree_node *tree_next (tree_root *root, tree_node *node)
{
    tree_node *next;
    int32_t compare;

    if (!node) {
        return (0);
    }

    if (node->right) {
        next = tree_first(node->right);
    } else if (node->parent) {
        next = node;
        do {
            next = next->parent;
            if (!next) {
                break;
            }

            compare = tree_node_compare_func(root, next, node);
        } while (compare <= 0);
    } else {
        next = 0;
    }

    return (next);
}

tree_node *tree_prev (tree_root *root, tree_node *node)
{
    tree_node *next;
    int32_t compare;

    if (!node) {
        return (0);
    }

    if (node->left) {
        next = tree_last(node->left);
    } else if (node->parent) {
        next = node;
        do {
            next = next->parent;
            if (!next) {
                break;
            }

            compare = tree_node_compare_func(root, next, node);
        } while (compare > 0);
    } else {
        next = 0;
    }

    return (next);
}

/*
 * Walk all nodes. Not safe if next node is destroyed. Use TREE_WALK instead.
 */
boolean tree_walk (tree_root *root, tree_walker_func walker_func, void *arg)
{
    tree_node *node;
    tree_node *next;

    next = node = tree_root_first(root);

    while (next) {
        node = next;
        next = tree_next(root, node);

        if (!(*walker_func)(node, arg)) {
            return (0);
        }
    }

    return (1);
}

/*
 * Walk all nodes. Not safe if next node is destroyed. Use TREE_WALK instead.
 */
boolean tree_walk_reverse (tree_root *root,
                           tree_walker_func walker_func, void *arg)
{
    tree_node *node;
    tree_node *prev;

    prev = node = tree_root_last(root);

    while (prev) {
        node = prev;
        prev = tree_prev(root, node);

        if (!(*walker_func)(node, arg)) {
            return (0);
        }
    }

    return (1);
}

/*
 * Find the nth node. Slow.
 */
tree_node *tree_root_get_nth (tree_root *root, uint32_t n)
{
    tree_node *node;
    tree_node *next;
    tree_node *top;
    uint32_t count;
    int32_t compare;

    top = root->node;
    if (!top) {
        return (0);
    }

    node = tree_first(top);
    count = 0;

    while (node) {
        if (count++ == n) {
            return (node);
        }

        if (node->right) {
            next = tree_first(node->right);
        } else if (node->parent) {
            next = node;
            do {
                next = next->parent;
                if (!next) {
                    break;
                }

                compare = tree_node_compare_func(root, next, node);
            } while (compare <= 0);
        } else {
            next = 0;
        }

        node = next;
    }

    return (0);
}

uint32_t tree_root_size (tree_root *root)
{
    tree_node *top;

    top = root->node;
    if (!top) {
        return (0);
    }

    return (tree_size(top));
}

tree_node *tree_root_get_random (tree_root *root)
{
    tree_node *top;
    uint32_t size;
    uint32_t r;

    top = root->node;
    if (!top) {
        return (0);
    }

    size = tree_size(top);
    if (!size) {
        return (0);
    }

    r = rand() % size;

    return (tree_root_get_nth(root, r));
}

#ifdef ENABLE_TREE_SANITY
/*
 * A node must be red or black.
 */
static void tree_verify_node_is_red_or_black (tree_node *node)
{
    ASSERT((tree_node_color(node) == RB_RED) ||
           (tree_node_color(node) == RB_BLACK));

    if (!node) {
        return;
    }

    ASSERT(node->left != node);
    ASSERT(node->right != node);
    ASSERT(node->parent != node);

    tree_verify_node_is_red_or_black(node->left);
    tree_verify_node_is_red_or_black(node->right);
}

/*
 * The root node must be black.
 */
static void tree_verify_root_is_black (tree_node *node)
{
    if (!node) {
        return;
    }

    ASSERT(tree_node_color(node) == RB_BLACK);
}

/*
 * Every red node has two black children and a black parent.
 */
static void
tree_verify_every_red_node_has_two_black_children (tree_node *node)
{
    if (tree_node_color(node) == RB_RED) {
        ASSERT(tree_node_color(node->left)   == RB_BLACK);
        ASSERT(tree_node_color(node->right)  == RB_BLACK);
        ASSERT(tree_node_color(node->parent) == RB_BLACK);
    }

    if (!node) {
        return;
    }

    ASSERT(node->left != node);
    ASSERT(node->right != node);
    ASSERT(node->parent != node);

    if (node->parent) {
        ASSERT((node->parent->left == node) ||
               (node->parent->right == node));
    }

    tree_verify_every_red_node_has_two_black_children(node->left);
    tree_verify_every_red_node_has_two_black_children(node->right);
}

/*
 * Count every black node path and make sure they are the same.
 */
static void
tree_verify_count_black_path (const tree_node * node,
                              int this_black_path_count,
                              int *first_saved_black_path_count)
{
    /*
     * Remember if this is a leaf node, it is black.
     */
    if (tree_node_color(node) == RB_BLACK) {
        this_black_path_count++;
    }

    /*
     * If we read a leaf node...
     */
    if (!node) {
        /*
         * And if we have no saved path count...
         */
        if (*first_saved_black_path_count == -1) {
            /*
             * Save it.
             */
            *first_saved_black_path_count = this_black_path_count;
        } else {
            /*
             * Else check this new path is the same.
             */
            ASSERT(this_black_path_count == *first_saved_black_path_count);
        }

        return;
    }

    /*
     * Check the children.
     */
    tree_verify_count_black_path(node->left, this_black_path_count,
                                 first_saved_black_path_count);

    tree_verify_count_black_path(node->right, this_black_path_count,
                                 first_saved_black_path_count);
}

/*
 * Count every black node path and make sure they are the same.
 */
static void
tree_verify_all_paths_have_same_black_node_count (const tree_node *root)
{
    int black_path_count;

    black_path_count = -1;

    tree_verify_count_black_path(root, 0, &black_path_count);
}
#endif

/*
 * Perform checks that the red black tree is valid.
 */
static void tree_verify (const tree_root *root)
{
#ifdef ENABLE_TREE_SANITY
    ASSERT(root);

    tree_verify_root_is_black(root->node);
    tree_verify_node_is_red_or_black(root->node);
    tree_verify_every_red_node_has_two_black_children(root->node);
    tree_verify_all_paths_have_same_black_node_count(root->node);
#endif
}

/*
 * Move the new node to the same position as the old node.
 */
static void tree_replace_node (tree_root *root,
                               tree_node *old_node,
                               tree_node *new_node)
{
    if (!old_node->parent) {
        /*
         * Old node was the root.
         */
        root->node = new_node;
    } else {
        /*
         * Make the old nodes parent see only the new node.
         */
        if (old_node == old_node->parent->left) {
            old_node->parent->left = new_node;
        } else {
            old_node->parent->right = new_node;
        }
    }

    /*
     * And make the new node only see the old node's parent.
     */
    if (new_node) {
        new_node->parent = old_node->parent;
    }
}

/*
 * Swap two nodes in the tree
 */
static void tree_swap_nodes (tree_root *root, tree_node *a, tree_node *b)
{
    tree_node A;
    tree_node B;
    tree_node PA;
    tree_node PB;

    A = *a;
    B = *b;

    /*
     * Handle root node change.
     */
    if (A.parent) {
        PA = *(A.parent);
    } else {
        root->node = b;
        PA.left = 0;
        PA.right = 0;
    }

    if (B.parent) {
        PB = *(B.parent);
    } else {
        root->node = a;
        PB.left = 0;
        PB.right = 0;
    }

    /*
     * Make the children point to new parents.
     */
    if (A.left) {
        A.left->parent = b;
    }

    if (A.right) {
        A.right->parent = b;
    }

    if (B.left) {
        B.left->parent = a;
    }

    if (B.right) {
        B.right->parent = a;
    }

    /*
     * Make the parents point to new children.
     */
    if (PA.left == a) {
        A.parent->left = b;
    } else if (PA.right == a) {
        A.parent->right = b;
    }

    if (PB.left == b) {
        B.parent->left = a;
    } else if (PB.right == b) {
        B.parent->right = a;
    }

    /*
     * Swap the nodes.
     */
    *a = B;
    *b = A;

    /*
     * Fixup for the case where nodes are immediately adjacent and the above
     * pointer swap causes loops.
     */
    if (a->left == a) {
        a->left = b;
    } else if (a->right == a) {
        a->right = b;
    } else if (a->parent == a) {
        a->parent = b;
    }

    if (b->left == b) {
        b->left = a;
    } else if (b->right == b) {
        b->right = a;
    } else if (b->parent == b) {
        b->parent = a;
    }
}

/*
 * Rotate a tree right.
 *
 *            A  <---- node         B
 *           / \                   / \
 *          /   \                 /   \
 *         /     \               /     \
 * succ-> B       C    ---->    D       A
 *       / \     / \           / \     / \
 *      /   \   /   \         /   \   /   \
 *     D     3 4     5       1     2 3     C
 *    / \                                 / \
 *   /   \                               /   \
 *  1     2                             4     5
 */
static void tree_rotate_right (tree_root *root, tree_node *node)
{
    tree_node *successor;

    successor = node->left;

    tree_replace_node(root, node, successor);
    node->left = successor->right;

    if (successor->right) {
        successor->right->parent = node;
    }

    successor->right = node;
    node->parent = successor;
}

/*
 * Rotate a tree left.
 *
 *            A                     B <---- node
 *           / \                   / \
 *          /   \                 /   \
 *         /     \               /     \
 *        B       C    <----    D       A <---- successor
 *       / \     / \           / \     / \
 *      /   \   /   \         /   \   /   \
 *     D     3 4     5       1     2 3     C
 *    / \                                 / \
 *   /   \                               /   \
 *  1     2                             4     5
 */
static void tree_rotate_left (tree_root *root, tree_node *node)
{
    tree_node *successor;

    successor = node->right;

    tree_replace_node(root, node, successor);

    /*
     *            A
     *           / \
     *          /   \
     *         /     \
     *        3       C
     *               / \
     *              /   \
     *             4     5
     */

    node->right = successor->left;

    if (successor->left) {
        successor->left->parent = node;
    }

    /*
     *        B
     *       / \
     *      /   \
     *     D     3
     *    / \
     *   /   \
     *  1     2
     *
     *
     *            A
     *             \
     *              \
     *               \
     *                C
     *               / \
     *              /   \
     *             4     5
     */

    successor->left = node;
    node->parent = successor;

    /*
     *            A
     *           / \
     *          /   \
     *         /     \
     *        B       C
     *       / \     / \
     *      /   \   /   \
     *     D     3 4     5
     *    / \
     *   /   \
     *  1     2
     */
}

/*
 * Insert a node.
 */
static boolean tree_insert_internal (tree_root *root,
                                     tree_node **root_node,
                                     tree_node *node)
{
    tree_node *top;
    int32_t compare;

    top = *root_node;
    if (!top) {
        *root_node = node;
        return (1);
    }

    for (;;) {

	compare = tree_node_compare_func(root, top, node);
        if (compare == 0) {
            /*
             * Duplicate node.
             */
	    return (0);
        }

        if (compare < 0) {
            if (top->right) {
                top = top->right;
                continue;
            }

            /*
             * Leaf node.
             */
            top->right = node;
            node->parent = top;
            break;
        }

        if (top->left) {
            top = top->left;
            continue;
        }

        /*
         * Leaf node.
         */
        top->left = node;
        node->parent = top;
        break;
    }

    return (1);
}

/*
 * Walk up the tree looking for red-red violations. Red-red means that one end
 * of the tree is now longer and needs balancing. The balancing will only need
 * one rotation to fix it.
 */
static void tree_balance (tree_root *root, tree_node *node)
{
    /*
     * If this node is the root node it is always black to indicate a path
     * length of 1.
     */
    if (!node->parent) {
        node->color = RB_BLACK;
        return;
    }

    /*
     * We recurse up the tree looking for red-red violations. If the parent
     * is black we can stop.
     */
    if (tree_node_color(node->parent) == RB_BLACK) {
        return;
    }

    /*
     * From here on we know we have a red-red violation.
     */

    /*
     * If the uncle is red we can recolor the grandparent and the B path
     * remains the same length. We need then to iterate up the tree to fix
     * any violations the recolor causes.
     *
     *             G(B)                  G(R)
     *             / \                   / \
     *            /   \                 /   \
     *           /     \               /     \
     *         P(R)    U(R)  ---->   P(B)    U(B)
     *         /                     /
     *        /                     /
     *      N(R)                  N(R)
     *
     *             G(B)                  G(R)
     *             / \                   / \
     *            /   \                 /   \
     *           /     \               /     \
     *         P(R)    U(R)  ---->   P(B)    U(B)
     *           \                     \
     *            \                     \
     *            N(R)                  N(R)
     */
    if (tree_node_color(uncle(node)) == RB_RED) {
        node->parent->color = RB_BLACK;
        uncle(node)->color = RB_BLACK;
        grandparent(node)->color = RB_RED;

        /*
         * Recurse up the tree to fix the grandparent who might now be causing
         * a red-red violation.
         */
        tree_balance(root, grandparent(node));
        return;
    }

    /*
     * We now need to handle the two cases where the uncle is red. First
     * simplify the tree to minimize the cases we need to deal with.
     *
     *              G                     G
     *             /                     /
     *            /                     /
     *           /                     /
     *          P            ---->    N
     *         / \                   / \
     *        /   \                 /   \
     *             N               P
     *            / \             / \
     *           /   \           /   \
     */
    if ((node == parent(node)->right) &&
        (parent(node) == grandparent(node)->left)) {

        tree_rotate_left(root, parent(node));
        node = node->left;
    } else if ((node == parent(node)->left) &&
               (node->parent == grandparent(node)->right)) {

        /*
         * Handle the mirror image of the above.
         */
        tree_rotate_right(root, parent(node));
        node = node->right;
    }

    /*
     * Now we can handle the uncle being red.
     *
     *             G(B)                  P(B)
     *             / \                   / \
     *            /   \                 /   \
     *           /     \               /     \
     *         P(R)    U(B)  ---->   N(R)    G(R)
     *         /                               \
     *        /                                 \
     *      N(R)                                U(B)
     */
    parent(node)->color = RB_BLACK;
    grandparent(node)->color = RB_RED;

    if ((node == parent(node)->left) &&
        (node->parent == grandparent(node)->left)) {

        tree_rotate_right(root, grandparent(node));
    } else {
        /*
         * Again mirror image.
         */
        ASSERT((node == parent(node)->right) &&
               (node->parent == grandparent(node)->right));

        tree_rotate_left(root, grandparent(node));
    }
}

/*
 * Insert a node.
 *
 * 1 on success.
 */
boolean tree_insert (tree_root *root, tree_node *node)
{
    if (!tree_insert_internal(root, &root->node, node)) {
        return (0);
    }

    /*
     * Any other new node begins as red. We now need to check for red-red
     * violations.
     */
    node->color = RB_RED;

    tree_balance(root, node);

    tree_verify(root);

    return (1);
}

/*
 * Insert a static memory node.
 *
 * 1 on success.
 */
boolean tree_insert_static (tree_root *root, tree_node *node)
{
    if (!tree_insert_internal(root, &root->node, node)) {
        return (0);
    }

    /*
     * Any other new node begins as red. We now need to check for red-red
     * violations.
     */
    node->color = RB_RED;
    node->is_static_mem = true;

    tree_balance(root, node);

    tree_verify(root);

    return (1);
}

/*
 * A black node is being deleted. Realance the tree.
 */
static void tree_black_node_removed (tree_root *root, tree_node * node)
{
    /*
     * If we deleted the root node then the tree is still balanced.
     */
    if (!node->parent) {
        return;
    }

    /*
     * We're deleting a black node. If we were deleting a red node it would
     * have had two children and we would have instead done a node swap.
     * Hence the presence of a sibling is guaranteed here as the black lenght
     * must be identical.
     */
    ASSERT(sibling(node));

    /*
     * Simplify the tree.
     */
    if (tree_node_color(sibling(node)) == RB_RED) {
        parent(node)->color = RB_RED;
        sibling(node)->color = RB_BLACK;
        if (node == parent(node)->left) {
            tree_rotate_left(root, node->parent);
        } else {
            tree_rotate_right(root, node->parent);
        }
    }

    /*
     * The sibling is black. As we're deleting one black node, if we
     * make the sibling red, it takes one black height out of the tree.
     */
    if (tree_node_color(node->parent) == RB_BLACK &&
        tree_node_color(sibling(node)) == RB_BLACK &&
        tree_node_color(sibling(node)->left) == RB_BLACK &&
        tree_node_color(sibling(node)->right) == RB_BLACK) {
        sibling(node)->color = RB_RED;
        tree_black_node_removed(root, node->parent);
        return;
    }

    /*
     * Similar to the above but the parent is red. We make the sibling
     * red and swap the red color from the parent.
     */
    if (tree_node_color(node->parent) == RB_RED &&
        tree_node_color(sibling(node)) == RB_BLACK &&
        tree_node_color(sibling(node)->left) == RB_BLACK &&
        tree_node_color(sibling(node)->right) == RB_BLACK) {
        sibling(node)->color = RB_RED;
        parent(node)->color = RB_BLACK;
        return;
    }

    if (node == parent(node)->left &&
        tree_node_color(sibling(node)) == RB_BLACK &&
        tree_node_color(sibling(node)->left) == RB_RED &&
        tree_node_color(sibling(node)->right) == RB_BLACK) {

        sibling(node)->color = RB_RED;
        sibling(node)->left->color = RB_BLACK;
        tree_rotate_right(root, sibling(node));

    } else if (node == parent(node)->right &&
            tree_node_color(sibling(node)) == RB_BLACK &&
            tree_node_color(sibling(node)->right) == RB_RED &&
            tree_node_color(sibling(node)->left) == RB_BLACK) {

        sibling(node)->color = RB_RED;
        sibling(node)->right->color = RB_BLACK;
        tree_rotate_left(root, sibling(node));
    }

    sibling(node)->color = tree_node_color(node->parent);
    parent(node)->color = RB_BLACK;

    if (node == parent(node)->left) {
        ASSERT(tree_node_color(sibling(node)->right) == RB_RED);
        sibling(node)->right->color = RB_BLACK;
        tree_rotate_left(root, node->parent);
        return;
    }

    ASSERT(tree_node_color(sibling(node)->left) == RB_RED);
    sibling(node)->left->color = RB_BLACK;
    tree_rotate_right(root, node->parent);
}

/*
 * Remove a node without doing a find.
 */
boolean tree_remove_found_node (tree_root *root, tree_node *node)
{
    tree_node *successor;
    tree_node *child;

    /*
     * If we have a single child we can just replace the node.
     *
     * However if we have two children we find the successor and replace
     * this node with that and then delete the successor which must only
     * have one child.
     */
    if (node->left && node->right) {
        /*
         * Check successor has one child only.
         */
        successor = tree_last(node->left);
        ASSERT(!successor->left || !successor->right);

        /*
         * Swap the two nodes
         */
        tree_swap_nodes(root, node, successor);
        ASSERT(successor->left && successor->right);
        ASSERT(!node->left || !node->right);
    }

    /*
     * We should now only have one child.
     */
    ASSERT(!node->left || !node->right);
    if (node->right) {
        child = node->right;
    } else {
        child = node->left;
    }

    /*
     * If the node we are deleting is black then we are changing the black
     * height of the tree and need to compensate.
     */
    if (tree_node_color(node) == RB_BLACK) {
        /*
         * If the child is red, force a red-red violation that must be
         * balanced.
         */
        node->color = tree_node_color(child);

        tree_black_node_removed(root, node);
    }

    /*
     * Now we've rebalanced, just replace this node.
     */
    tree_replace_node(root, node, child);

    /*
     * If this is now the root node, keep it black so we add one to all
     * path lenghts.
     */
    if (root->node) {
        root->node->color = RB_BLACK;
    }

    node->left = 0;
    node->right = 0;
    node->parent = 0;

    tree_verify(root);

    return (1);
}

/*
 * Remove a node.
 */
boolean tree_remove (tree_root *root, tree_node *node)
{
    node = tree_find(root, node);
    if (!node) {
        return (0);
    }

    return (tree_remove_found_node(root, node));
}

/*
 * Get rid of all nodes.
 */
void tree_empty (tree_root *root, tree_destroy_func func)
{
    tree_node *node;

    if (!root) {
        return;
    }

    for (;;) {
        node = tree_root_first(root);
        if (!node) {
            break;
        }

        tree_remove(root, node);

        if (func) {
            (*func)(node);
        }

        switch (root->type) {
            case TREE_KEY_CUSTOM:
                break;
            case TREE_KEY_INTEGER:
                break;
            case TREE_KEY_TWO_INTEGER:
                break;
            case TREE_KEY_POINTER:
                break;
            case TREE_KEY_STRING: {
                tree_key_string *A = (typeof(A))node;

                myfree(A->key);
            }
        }

        if (!node->is_static_mem) {
            myfree(node);
        }
    }
}

/*
 * Get rid of all nodes and the root.
 */
void tree_destroy (tree_root **rootp, tree_destroy_func func)
{
    tree_root *root;

    root = *rootp;
    if (!root) {
        return;
    }

    *rootp = 0;
    tree_empty(root, func);
    myfree(root);
}

static tree_root *
tree_alloc_internal (tree_key_type type,
                     tree_key_func compare_func,
                     const char *tree_name)
{
    tree_root *root;

    root = (typeof(root)) myzalloc(sizeof(*root), tree_name);
    root->type = type;
    root->compare_func = compare_func;

    return (root);
}

tree_root *tree_alloc (tree_key_type type,
                       const char *tree_name)
{
    return (tree_alloc_internal(type, 0, tree_name));
}

tree_root *tree_alloc_custom (tree_key_func compare_func,
                              const char *tree_name)
{
    return (tree_alloc_internal(TREE_KEY_CUSTOM, compare_func, tree_name));
}
