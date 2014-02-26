/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "db/DatabasePlugin.hxx"
#include "db/Selection.hxx"
#include "db/Helpers.hxx"
#include "db/LightDirectory.hxx"
#include "Directory.hxx"
#include "Song.hxx"
#include "SongFilter.hxx"
#include "DatabaseSave.hxx"
#include "db/DatabaseLock.hxx"
#include "db/DatabaseError.hxx"
#include "fs/TextFile.hxx"
#include "config/ConfigData.hxx"
#include "fs/FileSystem.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <errno.h>

static constexpr Domain simple_db_domain("simple_db");

inline SimpleDatabase::SimpleDatabase()
	:Database(simple_db_plugin),
	 path(AllocatedPath::Null()) {}

Database *
SimpleDatabase::Create(gcc_unused EventLoop &loop,
		       gcc_unused DatabaseListener &listener,
		       const config_param &param, Error &error)
{
	SimpleDatabase *db = new SimpleDatabase();
	if (!db->Configure(param, error)) {
		delete db;
		db = nullptr;
	}

	return db;
}

bool
SimpleDatabase::Configure(const config_param &param, Error &error)
{
	path = param.GetBlockPath("path", error);
	if (path.IsNull()) {
		if (!error.IsDefined())
			error.Set(simple_db_domain,
				  "No \"path\" parameter specified");
		return false;
	}

	path_utf8 = path.ToUTF8();

	return true;
}

bool
SimpleDatabase::Check(Error &error) const
{
	assert(!path.IsNull());

	/* Check if the file exists */
	if (!CheckAccess(path)) {
		/* If the file doesn't exist, we can't check if we can write
		 * it, so we are going to try to get the directory path, and
		 * see if we can write a file in that */
		const auto dirPath = path.GetDirectoryName();

		/* Check that the parent part of the path is a directory */
		struct stat st;
		if (!StatFile(dirPath, st)) {
			error.FormatErrno("Couldn't stat parent directory of db file "
					  "\"%s\"",
					  path_utf8.c_str());
			return false;
		}

		if (!S_ISDIR(st.st_mode)) {
			error.Format(simple_db_domain,
				     "Couldn't create db file \"%s\" because the "
				     "parent path is not a directory",
				     path_utf8.c_str());
			return false;
		}

#ifndef WIN32
		/* Check if we can write to the directory */
		if (!CheckAccess(dirPath, X_OK | W_OK)) {
			const int e = errno;
			const std::string dirPath_utf8 = dirPath.ToUTF8();
			error.FormatErrno(e, "Can't create db file in \"%s\"",
					  dirPath_utf8.c_str());
			return false;
		}
#endif
		return true;
	}

	/* Path exists, now check if it's a regular file */
	struct stat st;
	if (!StatFile(path, st)) {
		error.FormatErrno("Couldn't stat db file \"%s\"",
				  path_utf8.c_str());
		return false;
	}

	if (!S_ISREG(st.st_mode)) {
		error.Format(simple_db_domain,
			     "db file \"%s\" is not a regular file",
			     path_utf8.c_str());
		return false;
	}

#ifndef WIN32
	/* And check that we can write to it */
	if (!CheckAccess(path, R_OK | W_OK)) {
		error.FormatErrno("Can't open db file \"%s\" for reading/writing",
				  path_utf8.c_str());
		return false;
	}
#endif

	return true;
}

bool
SimpleDatabase::Load(Error &error)
{
	assert(!path.IsNull());
	assert(root != nullptr);

	TextFile file(path);
	if (file.HasFailed()) {
		error.FormatErrno("Failed to open database file \"%s\"",
				  path_utf8.c_str());
		return false;
	}

	if (!db_load_internal(file, *root, error))
		return false;

	struct stat st;
	if (StatFile(path, st))
		mtime = st.st_mtime;

	return true;
}

bool
SimpleDatabase::Open(Error &error)
{
	root = Directory::NewRoot();
	mtime = 0;

#ifndef NDEBUG
	borrowed_song_count = 0;
#endif

	if (!Load(error)) {
		delete root;

		LogError(error);
		error.Clear();

		if (!Check(error))
			return false;

		root = Directory::NewRoot();
	}

	return true;
}

void
SimpleDatabase::Close()
{
	assert(root != nullptr);
	assert(borrowed_song_count == 0);

	delete root;
}

const LightSong *
SimpleDatabase::GetSong(const char *uri, Error &error) const
{
	assert(root != nullptr);
	assert(borrowed_song_count == 0);

	db_lock();
	const Song *song = root->LookupSong(uri);
	db_unlock();
	if (song == nullptr) {
		error.Format(db_domain, DB_NOT_FOUND,
			     "No such song: %s", uri);
		return nullptr;
	}

	light_song = song->Export();

#ifndef NDEBUG
	++borrowed_song_count;
#endif

	return &light_song;
}

void
SimpleDatabase::ReturnSong(gcc_unused const LightSong *song) const
{
	assert(song == &light_song);

#ifndef NDEBUG
	assert(borrowed_song_count > 0);
	--borrowed_song_count;
#endif
}

bool
SimpleDatabase::Visit(const DatabaseSelection &selection,
		      VisitDirectory visit_directory,
		      VisitSong visit_song,
		      VisitPlaylist visit_playlist,
		      Error &error) const
{
	ScopeDatabaseLock protect;

	auto r = root->LookupDirectory(selection.uri.c_str());
	if (r.uri == nullptr) {
		/* it's a directory */

		if (selection.recursive && visit_directory &&
		    !visit_directory(r.directory->Export(), error))
			return false;

		return r.directory->Walk(selection.recursive, selection.filter,
					 visit_directory, visit_song,
					 visit_playlist,
					 error);
	}

	if (strchr(r.uri, '/') == nullptr) {
		if (visit_song) {
			Song *song = r.directory->FindSong(r.uri);
			if (song != nullptr) {
				const LightSong song2 = song->Export();
				return !selection.Match(song2) ||
					visit_song(song2, error);
			}
		}
	}

	error.Set(db_domain, DB_NOT_FOUND, "No such directory");
	return false;
}

bool
SimpleDatabase::VisitUniqueTags(const DatabaseSelection &selection,
				TagType tag_type,
				VisitString visit_string,
				Error &error) const
{
	return ::VisitUniqueTags(*this, selection, tag_type, visit_string,
				 error);
}

bool
SimpleDatabase::GetStats(const DatabaseSelection &selection,
			 DatabaseStats &stats, Error &error) const
{
	return ::GetStats(*this, selection, stats, error);
}

bool
SimpleDatabase::Save(Error &error)
{
	db_lock();

	LogDebug(simple_db_domain, "removing empty directories from DB");
	root->PruneEmpty();

	LogDebug(simple_db_domain, "sorting DB");
	root->Sort();

	db_unlock();

	LogDebug(simple_db_domain, "writing DB");

	FILE *fp = FOpen(path, FOpenMode::WriteText);
	if (!fp) {
		error.FormatErrno("unable to write to db file \"%s\"",
				  path_utf8.c_str());
		return false;
	}

	db_save_internal(fp, *root);

	if (ferror(fp)) {
		error.SetErrno("Failed to write to database file");
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
	DatabasePlugin::FLAG_REQUIRE_STORAGE,
	SimpleDatabase::Create,
};
