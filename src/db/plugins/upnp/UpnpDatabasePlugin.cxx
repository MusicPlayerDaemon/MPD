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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "UpnpDatabasePlugin.hxx"
#include "Directory.hxx"
#include "Tags.hxx"
#include "lib/upnp/ClientInit.hxx"
#include "lib/upnp/Discovery.hxx"
#include "lib/upnp/ContentDirectoryService.hxx"
#include "lib/upnp/Util.hxx"
#include "db/Interface.hxx"
#include "db/DatabasePlugin.hxx"
#include "db/Selection.hxx"
#include "db/DatabaseError.hxx"
#include "db/LightDirectory.hxx"
#include "db/LightSong.hxx"
#include "db/Stats.hxx"
#include "config/Block.hxx"
#include "tag/TagBuilder.hxx"
#include "tag/TagTable.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "fs/Traits.hxx"
#include "Log.hxx"
#include "SongFilter.hxx"

#include <string>
#include <vector>
#include <set>

#include <assert.h>
#include <string.h>

static const char *const rootid = "0";

class UpnpSong : public LightSong {
	std::string uri2, real_uri2;

	Tag tag2;

public:
	UpnpSong(UPnPDirObject &&object, std::string &&_uri)
		:uri2(std::move(_uri)),
		 real_uri2(std::move(object.url)),
		 tag2(std::move(object.tag)) {
		directory = nullptr;
		uri = uri2.c_str();
		real_uri = real_uri2.c_str();
		tag = &tag2;
		mtime = 0;
		start_time = end_time = SongTime::zero();
	}
};

class UpnpDatabase : public Database {
	UpnpClient_Handle handle;
	UPnPDeviceDirectory *discovery;

public:
	UpnpDatabase():Database(upnp_db_plugin) {}

	static Database *Create(EventLoop &loop, DatabaseListener &listener,
				const ConfigBlock &block);

	virtual void Open() override;
	virtual void Close() override;
	virtual const LightSong *GetSong(const char *uri_utf8) const override;
	void ReturnSong(const LightSong *song) const override;

	virtual bool Visit(const DatabaseSelection &selection,
			   VisitDirectory visit_directory,
			   VisitSong visit_song,
			   VisitPlaylist visit_playlist,
			   Error &error) const override;

	virtual bool VisitUniqueTags(const DatabaseSelection &selection,
				     TagType tag_type, tag_mask_t group_mask,
				     VisitTag visit_tag,
				     Error &error) const override;

	virtual bool GetStats(const DatabaseSelection &selection,
			      DatabaseStats &stats,
			      Error &error) const override;
	time_t GetUpdateStamp() const override {
		return 0;
	}

private:
	bool VisitServer(const ContentDirectoryService &server,
			 const std::list<std::string> &vpath,
			 const DatabaseSelection &selection,
			 VisitDirectory visit_directory,
			 VisitSong visit_song,
			 VisitPlaylist visit_playlist,
			 Error &error) const;

	/**
	 * Run an UPnP search according to MPD parameters, and
	 * visit_song the results.
	 */
	bool SearchSongs(const ContentDirectoryService &server,
			 const char *objid,
			 const DatabaseSelection &selection,
			 VisitSong visit_song,
			 Error &error) const;

	UPnPDirContent SearchSongs(const ContentDirectoryService &server,
				   const char *objid,
				   const DatabaseSelection &selection) const;

	UPnPDirObject Namei(const ContentDirectoryService &server,
			    const std::list<std::string> &vpath) const;

	/**
	 * Take server and objid, return metadata.
	 */
	UPnPDirObject ReadNode(const ContentDirectoryService &server,
			       const char *objid) const;

	/**
	 * Get the path for an object Id. This works much like pwd,
	 * except easier cause our inodes have a parent id. Not used
	 * any more actually (see comments in SearchSongs).
	 */
	std::string BuildPath(const ContentDirectoryService &server,
			      const UPnPDirObject& dirent) const;
};

Database *
UpnpDatabase::Create(gcc_unused EventLoop &loop,
		     gcc_unused DatabaseListener &listener,
		     const ConfigBlock &)
{
	return new UpnpDatabase();
}

void
UpnpDatabase::Open()
{
	UpnpClientGlobalInit(handle);

	discovery = new UPnPDeviceDirectory(handle);
	try {
		discovery->Start();
	} catch (...) {
		delete discovery;
		UpnpClientGlobalFinish();
		throw;
	}
}

void
UpnpDatabase::Close()
{
	delete discovery;
	UpnpClientGlobalFinish();
}

void
UpnpDatabase::ReturnSong(const LightSong *_song) const
{
	assert(_song != nullptr);

	UpnpSong *song = (UpnpSong *)const_cast<LightSong *>(_song);
	delete song;
}

// Get song info by path. We can receive either the id path, or the titles
// one
const LightSong *
UpnpDatabase::GetSong(const char *uri) const
{
	auto vpath = stringToTokens(uri, "/", true);
	if (vpath.size() < 2)
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "No such song");

	auto server = discovery->GetServer(vpath.front().c_str());

	vpath.pop_front();

	UPnPDirObject dirent;
	if (vpath.front() != rootid) {
		dirent = Namei(server, vpath);
	} else {
		dirent = ReadNode(server, vpath.back().c_str());
	}

	return new UpnpSong(std::move(dirent), uri);
}

/**
 * Double-quote a string, adding internal backslash escaping.
 */
static void
dquote(std::string &out, const char *in)
{
	out.push_back('"');

	for (; *in != 0; ++in) {
		switch(*in) {
		case '\\':
		case '"':
			out.push_back('\\');
			break;
		}

		out.push_back(*in);
	}

	out.push_back('"');
}

// Run an UPnP search, according to MPD parameters. Return results as
// UPnP items
UPnPDirContent
UpnpDatabase::SearchSongs(const ContentDirectoryService &server,
			  const char *objid,
			  const DatabaseSelection &selection) const
{
	const SongFilter *filter = selection.filter;
	if (selection.filter == nullptr)
		return UPnPDirContent();

	const auto searchcaps = server.getSearchCapabilities(handle);
	if (searchcaps.empty())
		return UPnPDirContent();

	std::string cond;
	for (const auto &item : filter->GetItems()) {
		switch (auto tag = item.GetTag()) {
		case LOCATE_TAG_ANY_TYPE:
			{
				if (!cond.empty()) {
					cond += " and ";
				}
				cond += '(';
				bool first(true);
				for (const auto& cap : searchcaps) {
					if (first)
						first = false;
					else
						cond += " or ";
					cond += cap;
					if (item.GetFoldCase()) {
						cond += " contains ";
					} else {
						cond += " = ";
					}
					dquote(cond, item.GetValue());
				}
				cond += ')';
			}
			break;

		default:
			/* Unhandled conditions like
			   LOCATE_TAG_BASE_TYPE or
			   LOCATE_TAG_FILE_TYPE won't have a
			   corresponding upnp prop, so they will be
			   skipped */
			if (tag == TAG_ALBUM_ARTIST)
				tag = TAG_ARTIST;

			// TODO: support LOCATE_TAG_ANY_TYPE etc.
			const char *name = tag_table_lookup(upnp_tags,
							    TagType(tag));
			if (name == nullptr)
				continue;

			if (!cond.empty()) {
				cond += " and ";
			}
			cond += name;

			/* FoldCase doubles up as contains/equal
			   switch. UpNP search is supposed to be
			   case-insensitive, but at least some servers
			   have the same convention as mpd (e.g.:
			   minidlna) */
			if (item.GetFoldCase()) {
				cond += " contains ";
			} else {
				cond += " = ";
			}
			dquote(cond, item.GetValue());
		}
	}

	return server.search(handle, objid, cond.c_str());
}

static bool
visitSong(const UPnPDirObject &meta, const char *path,
	  const DatabaseSelection &selection,
	  VisitSong visit_song, Error& error)
{
	if (!visit_song)
		return true;

	LightSong song;
	song.directory = nullptr;
	song.uri = path;
	song.real_uri = meta.url.c_str();
	song.tag = &meta.tag;
	song.mtime = 0;
	song.start_time = song.end_time = SongTime::zero();

	return !selection.Match(song) || visit_song(song, error);
}

/**
 * Build synthetic path based on object id for search results. The use
 * of "rootid" is arbitrary, any name that is not likely to be a top
 * directory name would fit.
 */
static std::string
songPath(const std::string &servername,
	 const std::string &objid)
{
	return servername + "/" + rootid + "/" + objid;
}

bool
UpnpDatabase::SearchSongs(const ContentDirectoryService &server,
			  const char *objid,
			  const DatabaseSelection &selection,
			  VisitSong visit_song,
			  Error &error) const
{
	if (!visit_song)
		return true;

	for (auto &dirent : SearchSongs(server, objid, selection).objects) {
		if (dirent.type != UPnPDirObject::Type::ITEM ||
		    dirent.item_class != UPnPDirObject::ItemClass::MUSIC)
			continue;

		// We get song ids as the result of the UPnP search. But our
		// client expects paths (e.g. we get 1$4$3788 from minidlna,
		// but we need to translate to /Music/All_Music/Satisfaction).
		// We can do this in two ways:
		//  - Rebuild a normal path using BuildPath() which is a kind of pwd
		//  - Build a bogus path based on the song id.
		// The first method is nice because the returned paths are pretty, but
		// it has two big problems:
		//  - The song paths are ambiguous: e.g. minidlna returns all search
		//    results as being from the "All Music" directory, which can
		//    contain several songs with the same title (but different objids)
		//  - The performance of BuildPath() is atrocious on very big
		//    directories, even causing timeouts in clients. And of
		//    course, 'All Music' is very big.
		// So we return synthetic and ugly paths based on the object id,
		// which we later have to detect.
		const std::string path = songPath(server.getFriendlyName(),
						  dirent.id);
		if (!visitSong(std::move(dirent), path.c_str(),
			       selection, visit_song,
			       error))
			return false;
	}

	return true;
}

UPnPDirObject
UpnpDatabase::ReadNode(const ContentDirectoryService &server,
		       const char *objid) const
{
	auto dirbuf = server.getMetadata(handle, objid);
	if (dirbuf.objects.size() != 1)
		throw std::runtime_error("Bad resource");

	return std::move(dirbuf.objects.front());
}

std::string
UpnpDatabase::BuildPath(const ContentDirectoryService &server,
			const UPnPDirObject& idirent) const
{
	const char *pid = idirent.id.c_str();
	std::string path;
	while (strcmp(pid, rootid) != 0) {
		auto dirent = ReadNode(server, pid);
		pid = dirent.parent_id.c_str();

		if (path.empty())
			path = dirent.name;
		else
			path = PathTraitsUTF8::Build(dirent.name.c_str(),
						     path.c_str());
	}

	return PathTraitsUTF8::Build(server.getFriendlyName(),
				     path.c_str());
}

// Take server and internal title pathname and return objid and metadata.
UPnPDirObject
UpnpDatabase::Namei(const ContentDirectoryService &server,
		    const std::list<std::string> &vpath) const
{
	if (vpath.empty())
		// looking for root info
		return ReadNode(server, rootid);

	std::string objid(rootid);

	// Walk the path elements, read each directory and try to find the next one
	for (auto i = vpath.begin(), last = std::prev(vpath.end());; ++i) {
		auto dirbuf = server.readDir(handle, objid.c_str());

		// Look for the name in the sub-container list
		UPnPDirObject *child = dirbuf.FindObject(i->c_str());
		if (child == nullptr)
			throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
					    "No such object");

		if (i == last)
			return std::move(*child);

		if (child->type != UPnPDirObject::Type::CONTAINER)
			throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
					    "Not a container");

		objid = std::move(child->id);
	}
}

static bool
VisitItem(const UPnPDirObject &object, const char *uri,
	  const DatabaseSelection &selection,
	  VisitSong visit_song, VisitPlaylist visit_playlist,
	  Error &error)
{
	assert(object.type == UPnPDirObject::Type::ITEM);

	switch (object.item_class) {
	case UPnPDirObject::ItemClass::MUSIC:
		return !visit_song ||
			visitSong(object, uri,
				  selection, visit_song, error);

	case UPnPDirObject::ItemClass::PLAYLIST:
		if (visit_playlist) {
			/* Note: I've yet to see a
			   playlist item (playlists
			   seem to be usually handled
			   as containers, so I'll
			   decide what to do when I
			   see one... */
		}

		return true;

	case UPnPDirObject::ItemClass::UNKNOWN:
		return true;
	}

	assert(false);
	gcc_unreachable();
}

static bool
VisitObject(const UPnPDirObject &object, const char *uri,
	    const DatabaseSelection &selection,
	    VisitDirectory visit_directory,
	    VisitSong visit_song,
	    VisitPlaylist visit_playlist,
	    Error &error)
{
	switch (object.type) {
	case UPnPDirObject::Type::UNKNOWN:
		assert(false);
		gcc_unreachable();

	case UPnPDirObject::Type::CONTAINER:
		return !visit_directory ||
			visit_directory(LightDirectory(uri, 0), error);

	case UPnPDirObject::Type::ITEM:
		return VisitItem(object, uri, selection,
				 visit_song, visit_playlist,
				 error);
	}

	assert(false);
	gcc_unreachable();
}

// vpath is a parsed and writeable version of selection.uri. There is
// really just one path parameter.
bool
UpnpDatabase::VisitServer(const ContentDirectoryService &server,
			  const std::list<std::string> &vpath,
			  const DatabaseSelection &selection,
			  VisitDirectory visit_directory,
			  VisitSong visit_song,
			  VisitPlaylist visit_playlist,
			  Error &error) const
{
	/* If the path begins with rootid, we know that this is a
	   song, not a directory (because that's how we set things
	   up). Just visit it. Note that the choice of rootid is
	   arbitrary, any value not likely to be the name of a top
	   directory would be ok. */
	/* !Note: this *can't* be handled by Namei further down,
	   because the path is not valid for traversal. Besides, it's
	   just faster to access the target node directly */
	if (!vpath.empty() && vpath.front() == rootid) {
		switch (vpath.size()) {
		case 1:
			return true;

		case 2:
			break;

		default:
			throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
					    "Not found");
		}

		if (visit_song) {
			auto dirent = ReadNode(server, vpath.back().c_str());

			if (dirent.type != UPnPDirObject::Type::ITEM ||
			    dirent.item_class != UPnPDirObject::ItemClass::MUSIC)
				throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
						    "Not found");

			std::string path = songPath(server.getFriendlyName(),
						    dirent.id);
			if (!visitSong(std::move(dirent), path.c_str(),
				       selection,
				       visit_song, error))
				return false;
		}
		return true;
	}

	// Translate the target path into an object id and the associated metadata.
	const auto tdirent = Namei(server, vpath);

	/* If recursive is set, this is a search... No use sending it
	   if the filter is empty. In this case, we implement limited
	   recursion (1-deep) here, which will handle the "add dir"
	   case. */
	if (selection.recursive && selection.filter)
		return SearchSongs(server, tdirent.id.c_str(), selection,
				   visit_song, error);

	const char *const base_uri = selection.uri.empty()
		? server.getFriendlyName()
		: selection.uri.c_str();

	if (tdirent.type == UPnPDirObject::Type::ITEM) {
		return VisitItem(tdirent, base_uri,
				 selection,
				 visit_song, visit_playlist,
				 error);
	}

	/* Target was a a container. Visit it. We could read slices
	   and loop here, but it's not useful as mpd will only return
	   data to the client when we're done anyway. */
	for (const auto &dirent : server.readDir(handle, tdirent.id.c_str()).objects) {
		const std::string uri = PathTraitsUTF8::Build(base_uri,
							      dirent.name.c_str());
		if (!VisitObject(dirent, uri.c_str(),
				 selection,
				 visit_directory,
				 visit_song, visit_playlist,
				 error))
			return false;
	}

	return true;
}

// Deal with the possibly multiple servers, call VisitServer if needed.
bool
UpnpDatabase::Visit(const DatabaseSelection &selection,
		    VisitDirectory visit_directory,
		    VisitSong visit_song,
		    VisitPlaylist visit_playlist,
		    Error &error) const
{
	auto vpath = stringToTokens(selection.uri, "/", true);
	if (vpath.empty()) {
		for (const auto &server : discovery->GetDirectories()) {
			if (visit_directory) {
				const LightDirectory d(server.getFriendlyName(), 0);
				if (!visit_directory(d, error))
					return false;
			}

			if (selection.recursive &&
			    !VisitServer(server, vpath, selection,
					 visit_directory, visit_song, visit_playlist,
					 error))
				return false;
		}

		return true;
	}

	// We do have a path: the first element selects the server
	std::string servername(std::move(vpath.front()));
	vpath.pop_front();

	auto server = discovery->GetServer(servername.c_str());
	return VisitServer(server, vpath, selection,
			   visit_directory, visit_song, visit_playlist, error);
}

bool
UpnpDatabase::VisitUniqueTags(const DatabaseSelection &selection,
			      TagType tag, gcc_unused tag_mask_t group_mask,
			      VisitTag visit_tag,
			      Error &error) const
{
	// TODO: use group_mask

	if (!visit_tag)
		return true;

	std::set<std::string> values;
	for (auto& server : discovery->GetDirectories()) {
		const auto dirbuf = SearchSongs(server, rootid, selection);

		for (const auto &dirent : dirbuf.objects) {
			if (dirent.type != UPnPDirObject::Type::ITEM ||
			    dirent.item_class != UPnPDirObject::ItemClass::MUSIC)
				continue;

			const char *value = dirent.tag.GetValue(tag);
			if (value != nullptr) {
#if CLANG_OR_GCC_VERSION(4,8)
				values.emplace(value);
#else
				values.insert(value);
#endif
			}
		}
	}

	for (const auto& value : values) {
		TagBuilder builder;
		builder.AddItem(tag, value.c_str());
		if (!visit_tag(builder.Commit(), error))
			return false;
	}

	return true;
}

bool
UpnpDatabase::GetStats(const DatabaseSelection &,
		       DatabaseStats &stats, Error &) const
{
	/* Note: this gets called before the daemonizing so we can't
	   reallyopen this would be a problem if we had real stats */
	stats.Clear();
	return true;
}

const DatabasePlugin upnp_db_plugin = {
	"upnp",
	0,
	UpnpDatabase::Create,
};
