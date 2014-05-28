/*
 * Copyright (C) 2013 Neil McGill
 *
 * See the LICENSE file for license.
 */

#include "config.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "main.h"
#include "string.h"

/*
 * dir_exists
 *
 * Does the requested dir exist?
 */
boolean dir_exists (const char *indir)
{
    struct stat buf;
    char *dir;
    boolean rc;

    /*
     * msys is useless and unless you remove the / from the end of the name
     * will not think it is a dir.
     */
    dir = dupstr(indir, __FUNCTION__);
    strchopc(dir, DCHAR);

    rc = 0;
    if (stat(dir, &buf) >= 0) {
        rc = S_ISDIR(buf. st_mode);
    }

    myfree(dir);

    return (rc);
}

/*
 * do_dirlist_recurse
 *
 * Worker function for dirlist_recurse
 */
static tree_root *do_dirlist_recurse (tree_root *root,
                                      const char *dir,
                                      const char *include_suffix,
                                      const char *exclude_suffix,
                                      boolean include_dirs)
{
    tree_file_node *node;
    struct dirent * e;
    char *tmp;
    DIR * d;

    d = opendir(dir);
    if (!d) {
        ERR("cannot list dir %s", dir);
        return (0);
    }

    while ((e = readdir(d))) {
        char *dir_and_file;
        const char *name;
        struct stat s;

        name = e->d_name;

        if (!strcmp(name, ".")) {
            continue;
        } else if (!strcmp(name, "..")) {
            continue;
        } else {
            char *tmp1;
            char *tmp2;

            tmp1 = strappend(dir, DSEP);
            tmp2 = strappend(tmp1, name);
            myfree(tmp1);

            dir_and_file = tmp2;
        }

        if (stat(dir_and_file, &s) < 0) {
            myfree(dir_and_file);
            continue;
        }

        if (!include_dirs) {
            if (exclude_suffix) {
                if (strstr(name, exclude_suffix)) {
                    myfree(dir_and_file);
                    continue;
                }
            }

            if (include_suffix) {
                if (!strstr(name, include_suffix)) {
                    myfree(dir_and_file);
                    continue;
                }
            }
        }

        /*
         * Get rid of any // from the path
         */
        tmp = strsub(dir_and_file, DSEP DSEP, DSEP);
        myfree(dir_and_file);
        dir_and_file = tmp;

        node = (typeof(node)) myzalloc(sizeof(*node), "TREE NODE: dirlist");
        node->is_file = !S_ISDIR(s.st_mode);

        /*
         * Static key so tree removals are easy.
         */
        node->tree.key = dupstr(dir_and_file, "TREE KEY: dirlist2");

        if (!tree_insert(root, &node->tree.node)) {
            DIE("insert dir %s", dir_and_file);
        }

        if (!node->is_file) {
            do_dirlist_recurse(root, dir_and_file, include_suffix,
                               exclude_suffix, include_dirs);
        }

        myfree(dir_and_file);
    }

    closedir(d);

    return (root);
}

/*
 * dirlist_recurse
 *
 * Build a recursive directory list and populate elements into a tree.
 */
tree_root *dirlist_recurse (const char *dir,
                            const char *include_suffix,
                            const char *exclude_suffix,
                            boolean include_dirs)
{
    tree_root *root;

    root = tree_alloc(TREE_KEY_STRING, "TREE ROOT: dirlist");

    do_dirlist_recurse(root, dir, include_suffix, exclude_suffix, include_dirs);

    return (root);
}

/*
 * dirlist_free
 *
 * Destroy a tree of directory names.
 */
void dirlist_free (tree_root **root)
{
    tree_destroy(root, (tree_destroy_func)0);
}

/*
 * do_mkdir
 *
 * Make a directory.
 */
static boolean do_mkdir (const char *dir, mode_t mode)
{
    if (dir_exists(dir)) {
        return (true);
    }

    if (mkdir(dir, mode)) {
        ERR("Failed fo mkdir [%s], error: %s", dir, strerror(errno));
        return (false);
    }

    return (true);
}

/*
 * mkpath
 *
 * Ensure all directories in path exist
 */
boolean mkpath (const char *path, uint32_t mode)
{
    char *copypath = dupstr(path, __FUNCTION__);
    boolean status;
    char *pp;
    char *sp;

    status = true;
    pp = copypath;

    while (status && (sp = strchr(pp, '/')) != 0) {
        if (sp != pp) {
            *sp = '\0';

            if (!do_mkdir(copypath, mode)) {
                status = false;
            }

            *sp = '/';
        }

        pp = sp + 1;
    }

    if (status) {
        status = do_mkdir(path, mode);
    }

    myfree(copypath);

    return (status);
}
