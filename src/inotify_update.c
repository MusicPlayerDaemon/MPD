/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h" /* must be first for large file support */
#include "inotify_update.h"
#include "inotify_source.h"
#include "inotify_queue.h"
#include "database.h"
#include "mapper.h"
#include "path.h"

#include <assert.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "inotify"

enum {
	IN_MASK = IN_ATTRIB|IN_CLOSE_WRITE|IN_CREATE|IN_DELETE|IN_DELETE_SELF
	|IN_MOVE|IN_MOVE_SELF
#ifdef IN_ONLYDIR
	|IN_ONLYDIR
#endif
};

struct watch_directory {
	struct watch_directory *parent;

	char *name;

	int descriptor;

	GList *children;
};

static struct mpd_inotify_source *inotify_source;

static struct watch_directory inotify_root;
static GTree *inotify_directories;

static gint
compare(gconstpointer a, gconstpointer b)
{
	if (a < b)
		return -1;
	else if (a > b)
		return 1;
	else
		return 0;
}

static void
tree_add_watch_directory(struct watch_directory *directory)
{
	g_tree_insert(inotify_directories,
		      GINT_TO_POINTER(directory->descriptor), directory);
}

static void
tree_remove_watch_directory(struct watch_directory *directory)
{
	bool found = g_tree_remove(inotify_directories,
				   GINT_TO_POINTER(directory->descriptor));
	assert(found);
}

static struct watch_directory *
tree_find_watch_directory(int wd)
{
	return g_tree_lookup(inotify_directories, GINT_TO_POINTER(wd));
}

static void
remove_watch_directory(struct watch_directory *directory)
{
	assert(directory != NULL);
	assert(directory->parent != NULL);
	assert(directory->parent->children != NULL);

	tree_remove_watch_directory(directory);

	while (directory->children != NULL)
		remove_watch_directory(directory->children->data);

	directory->parent->children =
		g_list_remove(directory->parent->children, directory);

	mpd_inotify_source_rm(inotify_source, directory->descriptor);
	g_free(directory->name);
	g_slice_free(struct watch_directory, directory);
}

static char *
watch_directory_get_uri_fs(const struct watch_directory *directory)
{
	char *parent_uri, *uri;

	if (directory->parent == NULL)
		return NULL;

	parent_uri = watch_directory_get_uri_fs(directory->parent);
	if (parent_uri == NULL)
		return g_strdup(directory->name);

	uri = g_strconcat(parent_uri, "/", directory->name, NULL);
	g_free(parent_uri);

	return uri;
}

/* we don't look at "." / ".." nor files with newlines in their name */
static bool skip_path(const char *path)
{
	return (path[0] == '.' && path[1] == 0) ||
		(path[0] == '.' && path[1] == '.' && path[2] == 0) ||
		strchr(path, '\n') != NULL;
}

static void
recursive_watch_subdirectories(struct watch_directory *directory,
			       const char *path_fs)
{
	GError *error = NULL;
	DIR *dir;
	struct dirent *ent;

	assert(directory != NULL);
	assert(path_fs != NULL);

	dir = opendir(path_fs);
	if (dir == NULL) {
		g_warning("Failed to open directory %s: %s",
			  path_fs, g_strerror(errno));
		return;
	}

	while ((ent = readdir(dir))) {
		char *child_path_fs;
		struct stat st;
		int ret;
		struct watch_directory *child;

		if (skip_path(ent->d_name))
			continue;

		child_path_fs = g_strconcat(path_fs, "/", ent->d_name, NULL);
		/* XXX what about symlinks? */
		ret = lstat(child_path_fs, &st);
		if (ret < 0) {
			g_warning("Failed to stat %s: %s",
				  child_path_fs, g_strerror(errno));
			g_free(child_path_fs);
			continue;
		}

		if (!S_ISDIR(st.st_mode)) {
			g_free(child_path_fs);
			continue;
		}

		ret = mpd_inotify_source_add(inotify_source, child_path_fs,
					     IN_MASK, &error);
		if (ret < 0) {
			g_warning("Failed to register %s: %s",
				  child_path_fs, error->message);
			g_error_free(error);
			error = NULL;
			g_free(child_path_fs);
			continue;
		}

		child = tree_find_watch_directory(ret);
		if (child != NULL) {
			/* already being watched */
			g_free(child_path_fs);
			continue;
		}

		child = g_slice_new(struct watch_directory);
		child->parent = directory;
		child->name = g_strdup(ent->d_name);
		child->descriptor = ret;
		child->children = NULL;

		directory->children = g_list_prepend(directory->children,
						     child);

		tree_add_watch_directory(child);

		recursive_watch_subdirectories(child, child_path_fs);
		g_free(child_path_fs);
	}

	closedir(dir);
}

static void
mpd_inotify_callback(int wd, unsigned mask,
		     G_GNUC_UNUSED const char *name, G_GNUC_UNUSED void *ctx)
{
	struct watch_directory *directory;
	char *uri_fs;

	/*g_debug("wd=%d mask=0x%x name='%s'", wd, mask, name);*/

	directory = tree_find_watch_directory(wd);
	if (directory == NULL)
		return;

	uri_fs = watch_directory_get_uri_fs(directory);

	if ((mask & (IN_DELETE_SELF|IN_MOVE_SELF)) != 0) {
		g_free(uri_fs);
		remove_watch_directory(directory);
		return;
	}

	if ((mask & (IN_ATTRIB|IN_CREATE|IN_MOVE)) != 0 &&
	    (mask & IN_ISDIR) != 0) {
		/* a sub directory was changed: register those in
		   inotify */
		char *root = map_directory_fs(db_get_root());
		char *path_fs;

		if (uri_fs != NULL) {
			path_fs = g_strconcat(root, "/", uri_fs, NULL);
			g_free(root);
		} else
			path_fs = root;

		recursive_watch_subdirectories(directory, path_fs);
		g_free(path_fs);
	}

	if ((mask & (IN_CLOSE_WRITE|IN_MOVE|IN_DELETE)) != 0) {
		/* a file was changed, or a direectory was
		   moved/deleted: queue a database update */
		char *uri_utf8 = uri_fs != NULL
			? fs_charset_to_utf8(uri_fs)
			: g_strdup("");

		if (uri_utf8 != NULL)
			/* this function will take care of freeing
			   uri_utf8 */
			mpd_inotify_enqueue(uri_utf8);
	}

	g_free(uri_fs);
}

void
mpd_inotify_init(void)
{
	struct directory *root;
	char *path;
	GError *error = NULL;

	g_debug("initializing inotify");

	root = db_get_root();
	if (root == NULL) {
		g_debug("no music directory configured");
		return;
	}

	path = map_directory_fs(root);
	if (path == NULL) {
		g_warning("mapper has failed");
		return;
	}

	inotify_source = mpd_inotify_source_new(mpd_inotify_callback, NULL,
						&error);
	if (inotify_source == NULL) {
		g_warning("%s", error->message);
		g_error_free(error);
		g_free(path);
		return;
	}

	inotify_root.name = path;
	inotify_root.descriptor = mpd_inotify_source_add(inotify_source, path,
							 IN_MASK, &error);
	if (inotify_root.descriptor < 0) {
		g_warning("%s", error->message);
		g_error_free(error);
		mpd_inotify_source_free(inotify_source);
		inotify_source = NULL;
		g_free(path);
		return;
	}

	inotify_directories = g_tree_new(compare);
	tree_add_watch_directory(&inotify_root);

	recursive_watch_subdirectories(&inotify_root, path);

	mpd_inotify_queue_init();

	g_debug("watching music directory");
}

static gboolean
free_watch_directory(G_GNUC_UNUSED gpointer key, gpointer value,
		     G_GNUC_UNUSED gpointer data)
{
	struct watch_directory *directory = value;

	g_free(directory->name);
	g_list_free(directory->children);

	if (directory != &inotify_root)
		g_slice_free(struct watch_directory, directory);

	return false;
}

void
mpd_inotify_finish(void)
{
	if (inotify_source == NULL)
		return;

	mpd_inotify_queue_finish();
	mpd_inotify_source_free(inotify_source);

	g_tree_foreach(inotify_directories, free_watch_directory, NULL);
	g_tree_destroy(inotify_directories);
}
