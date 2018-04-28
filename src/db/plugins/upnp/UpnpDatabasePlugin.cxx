/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "tag/Builder.hxx"
#include "tag/Table.hxx"
#include "tag/Mask.hxx"
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
		mtime = std::chrono::system_clock::time_point::min();
		start_time = end_time = SongTime::zero();
	}
	UpnpSong(const UPnPDirObject &object, const std::string &_uri)
		:uri2(_uri),
		 real_uri2(object.url),
		 tag2(object.tag) {
		directory = nullptr;
		uri = uri2.c_str();
		real_uri = real_uri2.c_str();
		tag = &tag2;
		mtime = std::chrono::system_clock::time_point::min();
		start_time = end_time = SongTime::zero();
	}
};

struct UpnpCache
{
	std::string deviceId;
	UPnPDirObject root;
	Mutex mutex;
	void Open(const std::string &id) {
		assert(!id.empty());

		if (deviceId != id) {
			deviceId = id;
			root.Clear();
			root.id = "0";
			root.type = UPnPDirObject::Type::CONTAINER;
		}
	}

	void Clear() {
		deviceId.clear();
		root.Clear();
		root.id = "0";
		root.type = UPnPDirObject::Type::CONTAINER;
	}
};
UpnpCache mCache;

void ClearUpnpCache()
{
	mCache.Clear();
}

class UpnpDatabase : public Database {
	EventLoop &event_loop;
	UpnpClient_Handle handle;
	UPnPDeviceDirectory *discovery = nullptr;

public:
	explicit UpnpDatabase(EventLoop &_event_loop)
		:Database(upnp_db_plugin),
		 event_loop(_event_loop) {}

	static Database *Create(EventLoop &main_event_loop,
				EventLoop &io_event_loop,
				DatabaseListener &listener,
				const ConfigBlock &block);

	void Open() override;
	void Close() override;
	const LightSong *GetSong(const char *uri_utf8) const override;
	void ReturnSong(const LightSong *song) const override;

	void Visit(const DatabaseSelection &selection,
		   VisitDirectoryInfo visit_directory_info,
		   VisitDirectory visit_directory,
		   VisitSong visit_song,
		   VisitPlaylist visit_playlist) const override;

	void VisitUniqueTags(const DatabaseSelection &selection,
			     TagType tag_type, TagMask group_mask,
			     VisitTag visit_tag) const override;

	DatabaseStats GetStats(const DatabaseSelection &selection) const override;

	std::chrono::system_clock::time_point GetUpdateStamp() const noexcept override {
		return std::chrono::system_clock::time_point::min();
	}

private:
	void VisitServer(const ContentDirectoryService &server,
			 const std::list<std::string> &vpath,
			 const DatabaseSelection &selection,
			 VisitDirectory visit_directory,
			 VisitSong visit_song,
			 VisitPlaylist visit_playlist) const;

	/**
	 * Run an UPnP search according to MPD parameters, and
	 * visit_song the results.
	 */
	void SearchSongs(const ContentDirectoryService &server,
			 const char *objid,
			 const DatabaseSelection &selection,
			 VisitSong visit_song) const;

	UPnPDirObject SearchSongs(const ContentDirectoryService &server,
				   const char *objid,
				   const DatabaseSelection &selection) const;
};

Database *
UpnpDatabase::Create(EventLoop &, EventLoop &io_event_loop,
		     gcc_unused DatabaseListener &listener,
		     const ConfigBlock &)
{
	return new UpnpDatabase(io_event_loop);
}

void
UpnpDatabase::Open()
{
	handle = UpnpClientGlobalInit();

	discovery = GetUpnpDiscovery();
	if (discovery == nullptr) {
		UpnpClientGlobalFinish();
		throw;
	}
}

void
UpnpDatabase::Close()
{
	if (discovery != nullptr) {
		discovery = nullptr;
		UpnpClientGlobalFinish();
	}
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
	auto vpath = stringToTokens(uri, '/');
	if (vpath.size() < 2)
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "No such song");

	auto server = discovery->GetServer(vpath.front().c_str());

	vpath.pop_front();

	mCache.Open(server.GetDeviceId());
	UPnPDirObject *s = mCache.root.LookupSong(vpath);
	if (s == nullptr) {
		auto vpath2 = vpath;
		vpath2.pop_back();
		mCache.root.Update(server, handle, vpath2);
		s = mCache.root.LookupSong(vpath);
		if (s == nullptr) {
			throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
						"No such song");
		}
	}
	return new UpnpSong(*s, uri);
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
UPnPDirObject
UpnpDatabase::SearchSongs(const ContentDirectoryService &server,
			  const char *objid,
			  const DatabaseSelection &selection) const
{
	const SongFilter *filter = selection.filter;
	if (selection.filter == nullptr)
		return UPnPDirObject();

	const auto searchcaps = server.getSearchCapabilities(handle);
	if (searchcaps.empty())
		return UPnPDirObject();

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

static void
visitSong(const UPnPDirObject &meta, const char *path,
	  const DatabaseSelection &selection,
	  VisitSong visit_song)
{
	if (!visit_song)
		return;

	LightSong song;
	song.directory = nullptr;
	song.uri = path;
	song.real_uri = meta.url.c_str();
	song.tag = &meta.tag;
	song.mtime = std::chrono::system_clock::time_point::min();
	song.start_time = song.end_time = SongTime::zero();

	if (selection.Match(song))
		visit_song(song);
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

void
UpnpDatabase::SearchSongs(const ContentDirectoryService &server,
			  const char *objid,
			  const DatabaseSelection &selection,
			  VisitSong visit_song) const
{
	if (!visit_song)
		return;

	for (auto &dirent : SearchSongs(server, objid, selection).childs) {
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
		visitSong(std::move(dirent), path.c_str(),
			  selection, visit_song);
	}
}

static void
VisitItem(const UPnPDirObject &object, const char *uri,
	  const DatabaseSelection &selection,
	  VisitSong visit_song, VisitPlaylist visit_playlist)
{
	assert(object.type == UPnPDirObject::Type::ITEM);

	switch (object.item_class) {
	case UPnPDirObject::ItemClass::MUSIC:
		if (visit_song)
			visitSong(object, uri, selection, visit_song);
		break;

	case UPnPDirObject::ItemClass::PLAYLIST:
		if (visit_playlist) {
			/* Note: I've yet to see a
			   playlist item (playlists
			   seem to be usually handled
			   as containers, so I'll
			   decide what to do when I
			   see one... */
		}

		break;

	case UPnPDirObject::ItemClass::UNKNOWN:
		break;
	default:
		break;
	}
}

static void
VisitObject(const UPnPDirObject &object, const char *uri,
	    const DatabaseSelection &selection,
	    VisitDirectory visit_directory,
	    VisitSong visit_song,
	    VisitPlaylist visit_playlist)
{
	switch (object.type) {
	case UPnPDirObject::Type::UNKNOWN:
		assert(false);
		gcc_unreachable();

	case UPnPDirObject::Type::CONTAINER:
		if (visit_directory)
			visit_directory(LightDirectory(uri,
						       std::chrono::system_clock::time_point::min()));
		break;

	case UPnPDirObject::Type::ITEM:
		VisitItem(object, uri, selection,
			  visit_song, visit_playlist);
		break;
	}
}

// Deal with the possibly multiple servers, call VisitServer if needed.
void
UpnpDatabase::Visit(const DatabaseSelection &selection,
		    gcc_unused VisitDirectoryInfo visit_directory_info,
		    VisitDirectory visit_directory,
		    VisitSong visit_song,
		    VisitPlaylist visit_playlist) const
{
	auto vpath = stringToTokens(selection.uri, '/');
	if (vpath.empty()) {
		throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    "please set vist path first");
	}

	// We do have a path: the first element selects the server
	std::string servername(std::move(vpath.front()));
	vpath.pop_front();

	auto server = discovery->GetServer(servername.c_str());
	mCache.Open(server.GetDeviceId());
	if (!vpath.empty() && vpath.front() == rootid) {
		UPnPDirObject *s = mCache.root.LookupSong(vpath);
		if (s == nullptr) {
			auto vpath2 = vpath;
			vpath2.pop_back();
			mCache.root.Update(server, handle, vpath2);
			s = mCache.root.LookupSong(vpath);
			if (s == nullptr) {
				throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
							"No such song");
			}
		}
		if (visit_song) {
			std::string path = songPath(server.getFriendlyName(),
							s->id);
			visitSong(*s, path.c_str(), selection, visit_song);
		}
		return;
	}

	UPnPDirObject *d = mCache.root.LookupDirectory(vpath);
	if (d == nullptr) {
		mCache.root.Update(server, handle, vpath, selection.window_end);
		d = mCache.root.LookupDirectory(vpath);
		if (d == nullptr) {
			throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
						"No such object");
		}
	} else if (d->childs.size() < d->total){
		d->Update(server, handle, selection.window_end);
	}
	if (visit_directory_info) {
		visit_directory_info(LightDirectory(selection.uri.c_str(), std::chrono::system_clock::time_point::min(), d->total));
	}
	for (unsigned i=selection.window_start, end=std::min((unsigned)d->childs.size(), selection.window_end);
		i<end; i++) {
		const auto &c = d->childs[i];
		const std::string uri = PathTraitsUTF8::Build(selection.uri.c_str(),
							      c.name.c_str());
		VisitObject(c, uri.c_str(), selection, visit_directory, visit_song, visit_playlist);
	}
}

void
UpnpDatabase::VisitUniqueTags(const DatabaseSelection &selection,
			      TagType tag, gcc_unused TagMask group_mask,
			      VisitTag visit_tag) const
{
	// TODO: use group_mask

	if (!visit_tag)
		return;

	std::set<std::string> values;
	for (auto& server : discovery->GetDirectories()) {
		const auto dirbuf = SearchSongs(server, rootid, selection);

		for (const auto &dirent : dirbuf.childs) {
			if (dirent.type != UPnPDirObject::Type::ITEM ||
			    dirent.item_class != UPnPDirObject::ItemClass::MUSIC)
				continue;

			const char *value = dirent.tag.GetValue(tag);
			if (value != nullptr) {
				values.emplace(value);
			}
		}
	}

	for (const auto& value : values) {
		TagBuilder builder;
		builder.AddItem(tag, value.c_str());
		visit_tag(builder.Commit());
	}
}

DatabaseStats
UpnpDatabase::GetStats(const DatabaseSelection &) const
{
	/* Note: this gets called before the daemonizing so we can't
	   reallyopen this would be a problem if we had real stats */
	DatabaseStats stats;
	stats.Clear();
	return stats;
}

const DatabasePlugin upnp_db_plugin = {
	"upnp",
	0,
	UpnpDatabase::Create,
};
