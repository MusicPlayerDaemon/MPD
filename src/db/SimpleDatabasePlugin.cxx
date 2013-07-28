/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#include "config.h"
#include "SimpleDatabasePlugin.hxx"
#include "DatabaseSelection.hxx"
#include "DatabaseHelpers.hxx"
#include "Directory.hxx"
#include "SongFilter.hxx"
#include "DatabaseSave.hxx"
#include "DatabaseLock.hxx"
#include "db_error.h"
#include "TextFile.hxx"
#include "conf.h"
#include "fs/FileSystem.hxx"

#include <sys/types.h>
#include <errno.h>

G_GNUC_CONST
static inline GQuark
simple_db_quark(void)
{
	return g_quark_from_static_string("simple_db");
}

Database *
SimpleDatabase::Create(const struct config_param *param, GError **error_r)
{
	SimpleDatabase *db = new SimpleDatabase();
	if (!db->Configure(param, error_r)) {
		delete db;
		db = NULL;
	}

	return db;
}

bool
SimpleDatabase::Configure(const struct config_param *param, GError **error_r)
{
	GError *error = NULL;

	char *_path = config_dup_block_path(param, "path", &error);
	if (_path == NULL) {
		if (error != NULL)
			g_propagate_error(error_r, error);
		else
			g_set_error(error_r, simple_db_quark(), 0,
				    "No \"path\" parameter specified");
		return false;
	}

	path = Path::FromUTF8(_path);
	path_utf8 = _path;

	free(_path);

	if (path.IsNull()) {
		g_set_error(error_r, simple_db_quark(), 0,
			    "Failed to convert database path to FS encoding");
		return false;
	}

	return true;
}

bool
SimpleDatabase::Check(GError **error_r) const
{
	assert(!path.IsNull());
	assert(!path.empty());

	/* Check if the file exists */
	if (!CheckAccess(path, F_OK)) {
		/* If the file doesn't exist, we can't check if we can write
		 * it, so we are going to try to get the directory path, and
		 * see if we can write a file in that */
		const Path dirPath = path.GetDirectoryName();

		/* Check that the parent part of the path is a directory */
		struct stat st;
		if (!StatFile(dirPath, st)) {
			g_set_error(error_r, simple_db_quark(), errno,
				    "Couldn't stat parent directory of db file "
				    "\"%s\": %s",
				    path_utf8.c_str(), g_strerror(errno));
			return false;
		}

		if (!S_ISDIR(st.st_mode)) {
			g_set_error(error_r, simple_db_quark(), 0,
				    "Couldn't create db file \"%s\" because the "
				    "parent path is not a directory",
				    path_utf8.c_str());
			return false;
		}

		/* Check if we can write to the directory */
		if (!CheckAccess(dirPath, X_OK | W_OK)) {
			int error = errno;
			const std::string dirPath_utf8 = dirPath.ToUTF8();
			g_set_error(error_r, simple_db_quark(), error,
				    "Can't create db file in \"%s\": %s",
				    dirPath_utf8.c_str(), g_strerror(error));
			return false;
		}

		return true;
	}

	/* Path exists, now check if it's a regular file */
	struct stat st;
	if (!StatFile(path, st)) {
		g_set_error(error_r, simple_db_quark(), errno,
			    "Couldn't stat db file \"%s\": %s",
			    path_utf8.c_str(), g_strerror(errno));
		return false;
	}

	if (!S_ISREG(st.st_mode)) {
		g_set_error(error_r, simple_db_quark(), 0,
			    "db file \"%s\" is not a regular file",
			    path_utf8.c_str());
		return false;
	}

	/* And check that we can write to it */
	if (!CheckAccess(path, R_OK | W_OK)) {
		g_set_error(error_r, simple_db_quark(), errno,
			    "Can't open db file \"%s\" for reading/writing: %s",
			    path_utf8.c_str(), g_strerror(errno));
		return false;
	}

	return true;
}

bool
SimpleDatabase::Load(GError **error_r)
{
	assert(!path.empty());
	assert(root != NULL);

	TextFile file(path);
	if (file.HasFailed()) {
		g_set_error(error_r, simple_db_quark(), errno,
			    "Failed to open database file \"%s\": %s",
			    path_utf8.c_str(), g_strerror(errno));
		return false;
	}

	if (!db_load_internal(file, root, error_r))
		return false;

	struct stat st;
	if (StatFile(path, st))
		mtime = st.st_mtime;

	return true;
}

bool
SimpleDatabase::Open(GError **error_r)
{
	root = Directory::NewRoot();
	mtime = 0;

#ifndef NDEBUG
	borrowed_song_count = 0;
#endif

	GError *error = NULL;
	if (!Load(&error)) {
		root->Free();

		g_warning("Failed to load database: %s", error->message);
		g_error_free(error);

		if (!Check(error_r))
			return false;

		root = Directory::NewRoot();
	}

	return true;
}

void
SimpleDatabase::Close()
{
	assert(root != NULL);
	assert(borrowed_song_count == 0);

	root->Free();
}

Song *
SimpleDatabase::GetSong(const char *uri, GError **error_r) const
{
	assert(root != NULL);

	db_lock();
	Song *song = root->LookupSong(uri);
	db_unlock();
	if (song == NULL)
		g_set_error(error_r, db_quark(), DB_NOT_FOUND,
			    "No such song: %s", uri);
#ifndef NDEBUG
	else
		++const_cast<unsigned &>(borrowed_song_count);
#endif

	return song;
}

void
SimpleDatabase::ReturnSong(gcc_unused Song *song) const
{
	assert(song != nullptr);

#ifndef NDEBUG
	assert(borrowed_song_count > 0);
	--const_cast<unsigned &>(borrowed_song_count);
#endif
}

G_GNUC_PURE
const Directory *
SimpleDatabase::LookupDirectory(const char *uri) const
{
	assert(root != NULL);
	assert(uri != NULL);

	ScopeDatabaseLock protect;
	return root->LookupDirectory(uri);
}

bool
SimpleDatabase::Visit(const DatabaseSelection &selection,
		      VisitDirectory visit_directory,
		      VisitSong visit_song,
		      VisitPlaylist visit_playlist,
		      GError **error_r) const
{
	ScopeDatabaseLock protect;

	const Directory *directory = root->LookupDirectory(selection.uri);
	if (directory == NULL) {
		if (visit_song) {
			Song *song = root->LookupSong(selection.uri);
			if (song != nullptr)
				return !selection.Match(*song) ||
					visit_song(*song, error_r);
		}

		g_set_error(error_r, db_quark(), DB_NOT_FOUND,
			    "No such directory");
		return false;
	}

	if (selection.recursive && visit_directory &&
	    !visit_directory(*directory, error_r))
		return false;

	return directory->Walk(selection.recursive, selection.filter,
			       visit_directory, visit_song, visit_playlist,
			       error_r);
}

bool
SimpleDatabase::VisitUniqueTags(const DatabaseSelection &selection,
				enum tag_type tag_type,
				VisitString visit_string,
				GError **error_r) const
{
	return ::VisitUniqueTags(*this, selection, tag_type, visit_string,
				 error_r);
}

bool
SimpleDatabase::GetStats(const DatabaseSelection &selection,
			 DatabaseStats &stats, GError **error_r) const
{
	return ::GetStats(*this, selection, stats, error_r);
}

bool
SimpleDatabase::Save(GError **error_r)
{
	db_lock();

	g_debug("removing empty directories from DB");
	root->PruneEmpty();

	g_debug("sorting DB");
	root->Sort();

	db_unlock();

	g_debug("writing DB");

	FILE *fp = FOpen(path, FOpenMode::WriteText);
	if (!fp) {
		g_set_error(error_r, simple_db_quark(), errno,
			    "unable to write to db file \"%s\": %s",
			    path_utf8.c_str(), g_strerror(errno));
		return false;
	}

	db_save_internal(fp, root);

	if (ferror(fp)) {
		g_set_error(error_r, simple_db_quark(), errno,
			    "Failed to write to database file: %s",
			    g_strerror(errno));
		fclose(fp);
		return false;
	}

	fclose(fp);

	struct stat st;
	if (StatFile(path, st))
		mtime = st.st_mtime;

	return true;
}

const DatabasePlugin simple_db_plugin = {
	"simple",
	SimpleDatabase::Create,
};
