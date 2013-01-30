/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "InotifyUpdate.hxx"
#include "InotifySource.hxx"
#include "InotifyQueue.hxx"
#include "Mapper.hxx"
#include "Main.hxx"
#include "fs/Path.hxx"

#include <glib.h>

#include <map>

#include <assert.h>
#include <sys/inotify.h>
#include <sys/stat.h>
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

struct WatchDirectory {
	WatchDirectory *parent;

	char *name;

	int descriptor;

	GList *children;

	WatchDirectory(WatchDirectory *_parent, const char *_name,
		       int _descriptor)
		:parent(_parent), name(g_strdup(_name)),
		 descriptor(_descriptor),
		 children(nullptr) {}
};

static InotifySource *inotify_source;
static InotifyQueue *inotify_queue;

static unsigned inotify_max_depth;
static WatchDirectory *inotify_root;
static std::map<int, WatchDirectory *> inotify_directories;

static void
tree_add_watch_directory(WatchDirectory *directory)
{
	inotify_directories.insert(std::make_pair(directory->descriptor,
						  directory));
}

static void
tree_remove_watch_directory(WatchDirectory *directory)
{
	auto i = inotify_directories.find(directory->descriptor);
	assert(i != inotify_directories.end());
	inotify_directories.erase(i);
}

static WatchDirectory *
tree_find_watch_directory(int wd)
{
	auto i = inotify_directories.find(wd);
	if (i == inotify_directories.end())
		return nullptr;

	return i->second;
}

static void
remove_watch_directory(WatchDirectory *directory)
{
	assert(directory != NULL);

	if (directory->parent == NULL) {
		g_warning("music directory was removed - "
			  "cannot continue to watch it");
		return;
	}

	assert(directory->parent->children != NULL);

	tree_remove_watch_directory(directory);

	while (directory->children != NULL)
		remove_watch_directory((WatchDirectory *)directory->children->data);

	directory->parent->children =
		g_list_remove(directory->parent->children, directory);

	inotify_source->Remove(directory->descriptor);
	g_free(directory->name);
	delete directory;
}

static char *
watch_directory_get_uri_fs(const WatchDirectory *directory)
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
recursive_watch_subdirectories(WatchDirectory *directory,
			       const char *path_fs, unsigned depth)
{
	GError *error = NULL;
	DIR *dir;
	struct dirent *ent;

	assert(directory != NULL);
	assert(depth <= inotify_max_depth);
	assert(path_fs != NULL);

	++depth;

	if (depth > inotify_max_depth)
		return;

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

		if (skip_path(ent->d_name))
			continue;

		child_path_fs = g_strconcat(path_fs, "/", ent->d_name, NULL);
		ret = stat(child_path_fs, &st);
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

		ret = inotify_source->Add(child_path_fs, IN_MASK, &error);
		if (ret < 0) {
			g_warning("Failed to register %s: %s",
				  child_path_fs, error->message);
			g_error_free(error);
			error = NULL;
			g_free(child_path_fs);
			continue;
		}

		WatchDirectory *child = tree_find_watch_directory(ret);
		if (child != NULL) {
			/* already being watched */
			g_free(child_path_fs);
			continue;
		}

		child = new WatchDirectory(directory, ent->d_name, ret);

		directory->children = g_list_prepend(directory->children,
						     child);

		tree_add_watch_directory(child);

		recursive_watch_subdirectories(child, child_path_fs, depth);
		g_free(child_path_fs);
	}

	closedir(dir);
}

G_GNUC_PURE
static unsigned
watch_directory_depth(const WatchDirectory *d)
{
	assert(d != NULL);

	unsigned depth = 0;
	while ((d = d->parent) != NULL)
		++depth;

	return depth;
}

static void
mpd_inotify_callback(int wd, unsigned mask,
		     G_GNUC_UNUSED const char *name, G_GNUC_UNUSED void *ctx)
{
	WatchDirectory *directory;
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
		const char *root = mapper_get_music_directory_fs().c_str();
		const char *path_fs;
		char *allocated = NULL;

		if (uri_fs != NULL)
			path_fs = allocated =
				g_strconcat(root, "/", uri_fs, NULL);
		else
			path_fs = root;

		recursive_watch_subdirectories(directory, path_fs,
					       watch_directory_depth(directory));
		g_free(allocated);
	}

	if ((mask & (IN_CLOSE_WRITE|IN_MOVE|IN_DELETE)) != 0 ||
	    /* at the maximum depth, we watch out for newly created
	       directories */
	    (watch_directory_depth(directory) == inotify_max_depth &&
	     (mask & (IN_CREATE|IN_ISDIR)) == (IN_CREATE|IN_ISDIR))) {
		/* a file was changed, or a directory was
		   moved/deleted: queue a database update */

		if (uri_fs != nullptr) {
			const std::string uri_utf8 = Path::ToUTF8(uri_fs);
			if (!uri_utf8.empty())
				inotify_queue->Enqueue(uri_utf8.c_str());
		}
		else
			inotify_queue->Enqueue("");
	}

	g_free(uri_fs);
}

void
mpd_inotify_init(unsigned max_depth)
{
	GError *error = NULL;

	g_debug("initializing inotify");

	const Path &path = mapper_get_music_directory_fs();
	if (path.IsNull()) {
		g_debug("no music directory configured");
		return;
	}

	inotify_source = InotifySource::Create(*main_loop,
					       mpd_inotify_callback, nullptr,
					       &error);
	if (inotify_source == NULL) {
		g_warning("%s", error->message);
		g_error_free(error);
		return;
	}

	inotify_max_depth = max_depth;

	int descriptor = inotify_source->Add(path.c_str(), IN_MASK, &error);
	if (descriptor < 0) {
		g_warning("%s", error->message);
		g_error_free(error);
		delete inotify_source;
		inotify_source = NULL;
		return;
	}

	inotify_root = new WatchDirectory(nullptr, path.c_str(), descriptor);

	tree_add_watch_directory(inotify_root);

	recursive_watch_subdirectories(inotify_root, path.c_str(), 0);

	inotify_queue = new InotifyQueue(*main_loop);

	g_debug("watching music directory");
}

void
mpd_inotify_finish(void)
{
	if (inotify_source == NULL)
		return;

	delete inotify_queue;
	delete inotify_source;

	for (auto i : inotify_directories) {
		WatchDirectory *directory = i.second;

		g_free(directory->name);
		g_list_free(directory->children);

		delete directory;
	}

	inotify_directories.clear();
}
