/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "Mount.hxx"
#include "db/DatabasePlugin.hxx"
#include "db/Selection.hxx"
#include "db/Helpers.hxx"
#include "db/Stats.hxx"
#include "db/UniqueTags.hxx"
#include "db/VHelper.hxx"
#include "db/LightDirectory.hxx"
#include "Directory.hxx"
#include "Song.hxx"
#include "DatabaseSave.hxx"
#include "db/DatabaseLock.hxx"
#include "db/DatabaseError.hxx"
#include "fs/io/TextFile.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/FileOutputStream.hxx"
#include "fs/FileInfo.hxx"
#include "config/Block.hxx"
#include "fs/FileSystem.hxx"
#include "util/CharUtil.hxx"
#include "util/Domain.hxx"
#include "util/ConstBuffer.hxx"
#include "util/RecursiveMap.hxx"
#include "Log.hxx"

#ifdef ENABLE_ZLIB
#include "lib/zlib/GzipOutputStream.hxx"
#endif

#include <cerrno>
#include <memory>

static constexpr Domain simple_db_domain("simple_db");

inline SimpleDatabase::SimpleDatabase(const ConfigBlock &block)
	:Database(simple_db_plugin),
	 path(block.GetPath("path")),
#ifdef ENABLE_ZLIB
	 compress(block.GetBlockValue("compress", true)),
#endif
	 hide_playlist_targets(block.GetBlockValue("hide_playlist_targets", true)),
	 cache_path(block.GetPath("cache_directory"))
{
	if (path.IsNull())
		throw std::runtime_error("No \"path\" parameter specified");

	path_utf8 = path.ToUTF8();
}

inline SimpleDatabase::SimpleDatabase(AllocatedPath &&_path,
#ifndef ENABLE_ZLIB
				      [[maybe_unused]]
#endif
				      bool _compress) noexcept
	:Database(simple_db_plugin),
	 path(std::move(_path)),
	 path_utf8(path.ToUTF8()),
#ifdef ENABLE_ZLIB
	 compress(_compress),
#endif
	 cache_path(nullptr)
{
}

DatabasePtr
SimpleDatabase::Create(EventLoop &, EventLoop &,
		       [[maybe_unused]] DatabaseListener &listener,
		       const ConfigBlock &block)
{
	return std::make_unique<SimpleDatabase>(block);
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

#ifndef _WIN32
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

#ifndef _WIN32
	/* And check that we can write to it */
	if (!CheckAccess(path, R_OK | W_OK))
		throw FormatErrno("Can't open db file \"%s\" for reading/writing",
				  path_utf8.c_str());
#endif
}

void
SimpleDatabase::Load()
{
	assert(!path.IsNull());
	assert(root != nullptr);

	TextFile file(path);

	LogDebug(simple_db_domain, "reading DB");

	db_load_internal(file, *root);

	FileInfo fi;
	if (GetFileInfo(path, fi))
		mtime = fi.GetModificationTime();
}

void
SimpleDatabase::Open()
{
	assert(prefixed_light_song == nullptr);

	root = Directory::NewRoot();
	mtime = std::chrono::system_clock::time_point::min();

#ifndef NDEBUG
	borrowed_song_count = 0;
#endif

	try {
		Load();
	} catch (...) {
		LogError(std::current_exception());

		delete root;

		Check();

		root = Directory::NewRoot();
	}
}

void
SimpleDatabase::Close() noexcept
{
	assert(root != nullptr);
	assert(prefixed_light_song == nullptr);
	assert(borrowed_song_count == 0);

	delete root;
}

const LightSong *
SimpleDatabase::GetSong(std::string_view uri) const
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
			r.directory->mounted_database->GetSong(r.rest);
		if (song == nullptr)
			return nullptr;

		prefixed_light_song =
			new PrefixedLightSong(*song, r.uri);
		r.directory->mounted_database->ReturnSong(song);
		return prefixed_light_song;
	}

	if (r.rest.empty())
		/* it's a directory */
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "No such song");

	if (r.rest.find('/') != std::string_view::npos)
		/* refers to a URI "below" the actual song */
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "No such song");

	const Song *song = r.directory->FindSong(r.rest);
	if (song == nullptr)
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "No such song");

	exported_song.Construct(song->Export());
	protect.unlock();

#ifndef NDEBUG
	++borrowed_song_count;
#endif

	return &exported_song.Get();
}

void
SimpleDatabase::ReturnSong([[maybe_unused]] const LightSong *song) const noexcept
{
	assert(song != nullptr);
	assert(song == prefixed_light_song || song == &exported_song.Get());

	if (prefixed_light_song != nullptr) {
		delete prefixed_light_song;
		prefixed_light_song = nullptr;
	} else {
#ifndef NDEBUG
		assert(borrowed_song_count > 0);
		--borrowed_song_count;
#endif

		exported_song.Destruct();
	}
}

gcc_const
static DatabaseSelection
CheckSelection(DatabaseSelection selection) noexcept
{
	selection.uri.clear();
	selection.filter = nullptr;
	return selection;
}

void
SimpleDatabase::Visit(const DatabaseSelection &selection,
		      VisitDirectory visit_directory,
		      VisitSong visit_song,
		      VisitPlaylist visit_playlist) const
{
	ScopeDatabaseLock protect;

	auto r = root->LookupDirectory(selection.uri);

	if (r.directory->IsMount()) {
		/* pass the request and the remaining uri to the mounted database */
		protect.unlock();

		WalkMount(r.uri, *(r.directory->mounted_database),
			  r.rest,
			  selection,
			  visit_directory, visit_song, visit_playlist);

		return;
	}

	DatabaseVisitorHelper helper(CheckSelection(selection), visit_song);

	if (r.rest.data() == nullptr) {
		/* it's a directory */

		if (selection.recursive && visit_directory)
			visit_directory(r.directory->Export());

		r.directory->Walk(selection.recursive, selection.filter,
				  hide_playlist_targets,
				  visit_directory, visit_song,
				  visit_playlist);
		helper.Commit();
		return;
	}

	if (r.rest.find('/') == std::string_view::npos) {
		if (visit_song) {
			const Song *song = r.directory->FindSong(r.rest);
			if (song != nullptr) {
				const auto song2 = song->Export();
				if (selection.Match(song2))
					visit_song(song2);

				helper.Commit();
				return;
			}
		}
	}

	throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
			    "No such directory");
}

RecursiveMap<std::string>
SimpleDatabase::CollectUniqueTags(const DatabaseSelection &selection,
				  ConstBuffer<TagType> tag_types) const
{
	return ::CollectUniqueTags(*this, selection, tag_types);
}

DatabaseStats
SimpleDatabase::GetStats(const DatabaseSelection &selection) const
{
	return ::GetStats(*this, selection);
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
		gzip = std::make_unique<GzipOutputStream>(*os);
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
SimpleDatabase::Mount(const char *uri, DatabasePtr db)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(uri != nullptr);
#endif
	assert(db != nullptr);
	assert(*uri != 0);

	ScopeDatabaseLock protect;

	auto r = root->LookupDirectory(uri);
	if (r.rest.data() == nullptr)
		throw DatabaseError(DatabaseErrorCode::CONFLICT,
				    "Already exists");

	if (r.rest.find('/') != std::string_view::npos)
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "Parent not found");

	Directory *mnt = r.directory->CreateChild(r.rest);
	mnt->mounted_database = std::move(db);
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
SimpleDatabase::Mount(const char *local_uri, const char *storage_uri)
{
	if (cache_path.IsNull())
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "No 'cache_directory' configured");

	std::string name(storage_uri);
	std::replace_if(name.begin(), name.end(), IsUnsafeChar, '_');

	const auto name_fs = AllocatedPath::FromUTF8Throw(name);

#ifndef ENABLE_ZLIB
	constexpr bool compress = false;
#endif
	auto db = std::make_unique<SimpleDatabase>(cache_path / name_fs,
						   compress);
	db->Open();

	bool exists = db->FileExists();

	Mount(local_uri, std::move(db));

	return exists;
}

inline DatabasePtr
SimpleDatabase::LockUmountSteal(const char *uri) noexcept
{
	ScopeDatabaseLock protect;

	auto r = root->LookupDirectory(uri);
	if (r.rest.data() != nullptr || !r.directory->IsMount())
		return nullptr;

	auto db = std::move(r.directory->mounted_database);
	r.directory->Delete();

	return db;
}

bool
SimpleDatabase::Unmount(const char *uri) noexcept
{
	auto db = LockUmountSteal(uri);
	if (db == nullptr)
		return false;

	db->Close();
	return true;
}

constexpr DatabasePlugin simple_db_plugin = {
	"simple",
	DatabasePlugin::FLAG_REQUIRE_STORAGE,
	SimpleDatabase::Create,
};
