/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "DatabaseSave.hxx"
#include "db/DatabaseLock.hxx"
#include "db/DatabaseError.hxx"
#include "fs/io/TextFile.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "fs/io/FileOutputStream.hxx"
#include "fs/FileInfo.hxx"
#include "config/Block.hxx"
#include "fs/FileSystem.hxx"
#include "util/CharUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#ifdef ENABLE_ZLIB
#include "fs/io/GzipOutputStream.hxx"
#endif

#include <memory>

#include <errno.h>

static constexpr Domain simple_db_domain("simple_db");

inline SimpleDatabase::SimpleDatabase(const ConfigBlock &block)
	:Database(simple_db_plugin),
	 path(block.GetPath("path")),
#ifdef ENABLE_ZLIB
	 compress(block.GetBlockValue("compress", true)),
#endif
	 cache_path(block.GetPath("cache_directory")),
	 prefixed_light_song(nullptr)
{
	if (path.IsNull())
		throw std::runtime_error("No \"path\" parameter specified");

	path_utf8 = path.ToUTF8();
}

inline SimpleDatabase::SimpleDatabase(AllocatedPath &&_path,
#ifndef ENABLE_ZLIB
				      gcc_unused
#endif
				      bool _compress)
	:Database(simple_db_plugin),
	 path(std::move(_path)),
	 path_utf8(path.ToUTF8()),
#ifdef ENABLE_ZLIB
	 compress(_compress),
#endif
	 cache_path(AllocatedPath::Null()),
	 prefixed_light_song(nullptr) {
}

Database *
SimpleDatabase::Create(gcc_unused EventLoop &loop,
		       gcc_unused DatabaseListener &listener,
		       const ConfigBlock &block, Error &)
{
	return new SimpleDatabase(block);
}

void
SimpleDatabase::Check() const
{
	assert(!path.IsNull());

	/* Check if the file exists */
	if (!PathExists(path)) {
		/* If the file doesn't exist, we can't check if we can write
		 * it, so we are going to try to get the directory path, and
		 * see if we can write a file in that */
		const auto dirPath = path.GetDirectoryName();

		/* Check that the parent part of the path is a directory */
		FileInfo fi;

		try {
			fi = FileInfo(dirPath);
		} catch (...) {
			std::throw_with_nested(std::runtime_error("On parent directory of db file"));
		}

		if (!fi.IsDirectory())
			throw std::runtime_error("Couldn't create db file \"" +
						 path_utf8 + "\" because the "
						 "parent path is not a directory");

#ifndef WIN32
		/* Check if we can write to the directory */
		if (!CheckAccess(dirPath, X_OK | W_OK)) {
			const int e = errno;
			const std::string dirPath_utf8 = dirPath.ToUTF8();
			throw FormatErrno(e, "Can't create db file in \"%s\"",
					  dirPath_utf8.c_str());
		}
#endif

		return;
	}

	/* Path exists, now check if it's a regular file */
	const FileInfo fi(path);

	if (!fi.IsRegular())
		throw std::runtime_error("db file \"" + path_utf8 + "\" is not a regular file");

#ifndef WIN32
	/* And check that we can write to it */
	if (!CheckAccess(path, R_OK | W_OK))
		throw FormatErrno("Can't open db file \"%s\" for reading/writing",
				  path_utf8.c_str());
#endif
}

bool
SimpleDatabase::Load(Error &error)
{
	assert(!path.IsNull());
	assert(root != nullptr);

	TextFile file(path);

	if (!db_load_internal(file, *root, error))
		return false;

	FileInfo fi;
	if (GetFileInfo(path, fi))
		mtime = fi.GetModificationTime();

	return true;
}

void
SimpleDatabase::Open()
{
	assert(prefixed_light_song == nullptr);

	root = Directory::NewRoot();
	mtime = 0;

#ifndef NDEBUG
	borrowed_song_count = 0;
#endif

	try {
		Error error2;
		if (!Load(error2)) {
			LogError(error2);

			delete root;

			Check();

			root = Directory::NewRoot();
		}
	} catch (const std::exception &e) {
		LogError(e);

		delete root;

		Check();

		root = Directory::NewRoot();
	}
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
SimpleDatabase::GetSong(const char *uri) const
{
	assert(root != nullptr);
	assert(prefixed_light_song == nullptr);
	assert(borrowed_song_count == 0);

	ScopeDatabaseLock protect;

	auto r = root->LookupDirectory(uri);

	if (r.directory->IsMount()) {
		/* pass the request to the mounted database */
		protect.unlock();

		const LightSong *song =
			r.directory->mounted_database->GetSong(r.uri);
		if (song == nullptr)
			return nullptr;

		prefixed_light_song =
			new PrefixedLightSong(*song, r.directory->GetPath());
		return prefixed_light_song;
	}

	if (r.uri == nullptr)
		/* it's a directory */
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "No such song");

	if (strchr(r.uri, '/') != nullptr)
		/* refers to a URI "below" the actual song */
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "No such song");

	const Song *song = r.directory->FindSong(r.uri);
	protect.unlock();
	if (song == nullptr)
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "No such song");

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

	throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
			    "No such directory");
}

bool
SimpleDatabase::VisitUniqueTags(const DatabaseSelection &selection,
				TagType tag_type, tag_mask_t group_mask,
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

void
SimpleDatabase::Save()
{
	{
		const ScopeDatabaseLock protect;

		LogDebug(simple_db_domain, "removing empty directories from DB");
		root->PruneEmpty();

		LogDebug(simple_db_domain, "sorting DB");
		root->Sort();
	}

	LogDebug(simple_db_domain, "writing DB");

	FileOutputStream fos(path);

	OutputStream *os = &fos;

#ifdef ENABLE_ZLIB
	std::unique_ptr<GzipOutputStream> gzip;
	if (compress) {
		gzip.reset(new GzipOutputStream(*os));
		os = gzip.get();
	}
#endif

	BufferedOutputStream bos(*os);

	db_save_internal(bos, *root);

	bos.Flush();

#ifdef ENABLE_ZLIB
	if (gzip != nullptr) {
		gzip->Flush();
		gzip.reset();
	}
#endif

	fos.Commit();

	FileInfo fi;
	if (GetFileInfo(path, fi))
		mtime = fi.GetModificationTime();
}

void
SimpleDatabase::Mount(const char *uri, Database *db)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(uri != nullptr);
	assert(db != nullptr);
#endif
	assert(*uri != 0);

	ScopeDatabaseLock protect;

	auto r = root->LookupDirectory(uri);
	if (r.uri == nullptr)
		throw DatabaseError(DatabaseErrorCode::CONFLICT,
				    "Already exists");

	if (strchr(r.uri, '/') != nullptr)
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "Parent not found");

	Directory *mnt = r.directory->CreateChild(r.uri);
	mnt->mounted_database = db;
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
	if (cache_path.IsNull())
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "No 'cache_directory' configured");

	std::string name(storage_uri);
	std::replace_if(name.begin(), name.end(), IsUnsafeChar, '_');

	const auto name_fs = AllocatedPath::FromUTF8(name.c_str(), error);
	if (name_fs.IsNull())
		return false;

#ifndef ENABLE_ZLIB
	constexpr bool compress = false;
#endif
	auto db = new SimpleDatabase(AllocatedPath::Build(cache_path,
							  name_fs.c_str()),
				     compress);
	try {
		db->Open();
	} catch (...) {
		delete db;
		throw;
	}

	// TODO: update the new database instance?

	try {
		Mount(local_uri, db);
	} catch (...) {
		db->Close();
		delete db;
		throw;
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
