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
#include "PrefixedLightSong.hxx"
#include "db/DatabasePlugin.hxx"
#include "db/Selection.hxx"
#include "db/Helpers.hxx"
#include "db/UniqueTags.hxx"
#include "db/LightDirectory.hxx"
#include "Directory.hxx"
#include "Song.hxx"
#include "SongFilter.hxx"
#include "DatabaseSave.hxx"
#include "db/DatabaseLock.hxx"
#include "db/DatabaseError.hxx"
#include "fs/io/TextFile.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "fs/io/FileOutputStream.hxx"
#include "config/ConfigData.hxx"
#include "fs/FileSystem.hxx"
#include "util/CharUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#ifdef HAVE_ZLIB
#include "fs/io/GzipOutputStream.hxx"
#endif

#include <errno.h>

static constexpr Domain simple_db_domain("simple_db");

inline SimpleDatabase::SimpleDatabase()
	:Database(simple_db_plugin),
	 path(AllocatedPath::Null()),
#ifdef HAVE_ZLIB
	 compress(true),
#endif
	 cache_path(AllocatedPath::Null()),
	 prefixed_light_song(nullptr) {}

inline SimpleDatabase::SimpleDatabase(AllocatedPath &&_path,
#ifndef HAVE_ZLIB
				      gcc_unused
#endif
				      bool _compress)
	:Database(simple_db_plugin),
	 path(std::move(_path)),
	 path_utf8(path.ToUTF8()),
#ifdef HAVE_ZLIB
	 compress(_compress),
#endif
	 cache_path(AllocatedPath::Null()),
	 prefixed_light_song(nullptr) {
}

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

	cache_path = param.GetBlockPath("cache_directory", error);
	if (path.IsNull() && error.IsDefined())
		return false;

#ifdef HAVE_ZLIB
	compress = param.GetBlockValue("compress", compress);
#endif

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

	TextFile file(path, error);
	if (file.HasFailed())
		return false;

	if (!db_load_internal(file, *root, error) || !file.Check(error))
		return false;

	struct stat st;
	if (StatFile(path, st))
		mtime = st.st_mtime;

	return true;
}

bool
SimpleDatabase::Open(Error &error)
{
	assert(prefixed_light_song == nullptr);

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
	assert(prefixed_light_song == nullptr);
	assert(borrowed_song_count == 0);

	delete root;
}

const LightSong *
SimpleDatabase::GetSong(const char *uri, Error &error) const
{
	assert(root != nullptr);
	assert(prefixed_light_song == nullptr);
	assert(borrowed_song_count == 0);

	db_lock();

	auto r = root->LookupDirectory(uri);

	if (r.directory->IsMount()) {
		/* pass the request to the mounted database */
		db_unlock();

		const LightSong *song =
			r.directory->mounted_database->GetSong(r.uri, error);
		if (song == nullptr)
			return nullptr;

		prefixed_light_song =
			new PrefixedLightSong(*song, r.directory->GetPath());
		return prefixed_light_song;
	}

	if (r.uri == nullptr) {
		/* it's a directory */
		db_unlock();
		error.Format(db_domain, DB_NOT_FOUND,
			     "No such song: %s", uri);
		return nullptr;
	}

	if (strchr(r.uri, '/') != nullptr) {
		/* refers to a URI "below" the actual song */
		db_unlock();
		error.Format(db_domain, DB_NOT_FOUND,
			     "No such song: %s", uri);
		return nullptr;
	}

	const Song *song = r.directory->FindSong(r.uri);
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
	assert(song != nullptr);
	assert(song == &light_song || song == prefixed_light_song);

	delete prefixed_light_song;
	prefixed_light_song = nullptr;

#ifndef NDEBUG
	if (song == &light_song) {
		assert(borrowed_song_count > 0);
		--borrowed_song_count;
	}
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
				TagType tag_type, uint32_t group_mask,
				VisitTag visit_tag,
				Error &error) const
{
	return ::VisitUniqueTags(*this, selection, tag_type, group_mask,
				 visit_tag,
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

	FileOutputStream fos(path, error);
	if (!fos.IsDefined())
		return false;

	OutputStream *os = &fos;

#ifdef HAVE_ZLIB
	GzipOutputStream *gzip = nullptr;
	if (compress) {
		gzip = new GzipOutputStream(*os, error);
		if (!gzip->IsDefined()) {
			delete gzip;
			return false;
		}

		os = gzip;
	}
#endif

	BufferedOutputStream bos(*os);

	db_save_internal(bos, *root);

	if (!bos.Flush(error)) {
#ifdef HAVE_ZLIB
		delete gzip;
#endif
		return false;
	}

#ifdef HAVE_ZLIB
	if (gzip != nullptr) {
		bool success = gzip->Flush(error);
		delete gzip;
		if (!success)
			return false;
	}
#endif

	if (!fos.Commit(error))
		return false;

	struct stat st;
	if (StatFile(path, st))
		mtime = st.st_mtime;

	return true;
}

bool
SimpleDatabase::Mount(const char *uri, Database *db, Error &error)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(uri != nullptr);
	assert(db != nullptr);
#endif
	assert(*uri != 0);

	ScopeDatabaseLock protect;

	auto r = root->LookupDirectory(uri);
	if (r.uri == nullptr) {
		error.Format(db_domain, DB_CONFLICT,
			     "Already exists: %s", uri);
		return false;
	}

	if (strchr(r.uri, '/') != nullptr) {
		error.Format(db_domain, DB_NOT_FOUND,
			     "Parent not found: %s", uri);
		return false;
	}

	Directory *mnt = r.directory->CreateChild(r.uri);
	mnt->mounted_database = db;
	return true;
}

static constexpr bool
IsSafeChar(char ch)
{
	return IsAlphaNumericASCII(ch) || ch == '-' || ch == '_' || ch == '%';
}

static constexpr bool
IsUnsafeChar(char ch)
{
	return !IsSafeChar(ch);
}

bool
SimpleDatabase::Mount(const char *local_uri, const char *storage_uri,
		      Error &error)
{
	if (cache_path.IsNull()) {
		error.Format(db_domain, DB_NOT_FOUND,
			     "No 'cache_directory' configured");
		return false;
	}

	std::string name(storage_uri);
	std::replace_if(name.begin(), name.end(), IsUnsafeChar, '_');

#ifndef HAVE_ZLIB
	constexpr bool compress = false;
#endif
	auto db = new SimpleDatabase(AllocatedPath::Build(cache_path,
							  name.c_str()),
				     compress);
	if (!db->Open(error)) {
		delete db;
		return false;
	}

	// TODO: update the new database instance?

	if (!Mount(local_uri, db, error)) {
		db->Close();
		delete db;
		return false;
	}

	return true;
}

Database *
SimpleDatabase::LockUmountSteal(const char *uri)
{
	ScopeDatabaseLock protect;

	auto r = root->LookupDirectory(uri);
	if (r.uri != nullptr || !r.directory->IsMount())
		return nullptr;

	Database *db = r.directory->mounted_database;
	r.directory->mounted_database = nullptr;
	r.directory->Delete();

	return db;
}

bool
SimpleDatabase::Unmount(const char *uri)
{
	Database *db = LockUmountSteal(uri);
	if (db == nullptr)
		return false;

	db->Close();
	delete db;
	return true;
}

const DatabasePlugin simple_db_plugin = {
	"simple",
	DatabasePlugin::FLAG_REQUIRE_STORAGE,
	SimpleDatabase::Create,
};
