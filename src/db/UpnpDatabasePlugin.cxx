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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "UpnpDatabasePlugin.hxx"
#include "upnp/Domain.hxx"
#include "upnp/upnpplib.hxx"
#include "upnp/Discovery.hxx"
#include "upnp/ContentDirectoryService.hxx"
#include "upnp/Directory.hxx"
#include "upnp/Util.hxx"
#include "LazyDatabase.hxx"
#include "DatabasePlugin.hxx"
#include "DatabaseSelection.hxx"
#include "DatabaseError.hxx"
#include "PlaylistVector.hxx"
#include "Directory.hxx"
#include "Song.hxx"
#include "ConfigData.hxx"
#include "tag/TagBuilder.hxx"
#include "tag/TagTable.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "SongFilter.hxx"

#include <string>
#include <vector>
#include <map>
#include <set>

#include <assert.h>
#include <string.h>

static const char *const rootid = "0";

static const struct tag_table upnp_tags[] = {
	{ "upnp:artist", TAG_ARTIST },
	{ "upnp:album", TAG_ALBUM },
	{ "upnp:originalTrackNumber", TAG_TRACK },
	{ "upnp:genre", TAG_GENRE },
	{ "dc:title", TAG_TITLE },

	/* sentinel */
	{ nullptr, TAG_NUM_OF_ITEM_TYPES }
};

class UpnpDatabase : public Database {
	LibUPnP *m_lib;
	UPnPDeviceDirectory *m_superdir;
	Directory *m_root;

public:
	UpnpDatabase()
		: m_lib(0), m_superdir(0), m_root(0)
	{}

	static Database *Create(EventLoop &loop, DatabaseListener &listener,
				const config_param &param,
				Error &error);

	virtual bool Open(Error &error) override;
	virtual void Close() override;
	virtual Song *GetSong(const char *uri_utf8,
			      Error &error) const override;
	virtual void ReturnSong(Song *song) const;

	virtual bool Visit(const DatabaseSelection &selection,
			   VisitDirectory visit_directory,
			   VisitSong visit_song,
			   VisitPlaylist visit_playlist,
			   Error &error) const override;

	virtual bool VisitUniqueTags(const DatabaseSelection &selection,
				     TagType tag_type,
				     VisitString visit_string,
				     Error &error) const override;

	virtual bool GetStats(const DatabaseSelection &selection,
			      DatabaseStats &stats,
			      Error &error) const override;
	virtual time_t GetUpdateStamp() const {return 0;}

protected:
	bool Configure(const config_param &param, Error &error);

private:
	bool VisitServer(ContentDirectoryService* server,
			 const std::vector<std::string> &vpath,
			 const DatabaseSelection &selection,
			 VisitDirectory visit_directory,
			 VisitSong visit_song,
			 VisitPlaylist visit_playlist,
			 Error &error) const;

	/**
	 * Run an UPnP search according to MPD parameters, and
	 * visit_song the results.
	 */
	bool SearchSongs(ContentDirectoryService* server,
			 const char *objid,
			 const DatabaseSelection &selection,
			 VisitSong visit_song,
			 Error &error) const;

	bool SearchSongs(ContentDirectoryService* server,
			 const char *objid,
			 const DatabaseSelection &selection,
			 UPnPDirContent& dirbuf,
			 Error &error) const;

	bool Namei(ContentDirectoryService* server,
		   const std::vector<std::string> &vpath,
		   std::string &oobjid, UPnPDirObject &dirent,
		   Error &error) const;

	/**
	 * Take server and objid, return metadata.
	 */
	bool ReadNode(ContentDirectoryService* server,
		      const char *objid, UPnPDirObject& dirent,
		      Error &error) const;

	/**
	 * Get the path for an object Id. This works much like pwd,
	 * except easier cause our inodes have a parent id. Not used
	 * any more actually (see comments in SearchSongs).
	 */
	bool BuildPath(ContentDirectoryService* server,
		       const UPnPDirObject& dirent, std::string &idpath,
		       Error &error) const;
};

Database *
UpnpDatabase::Create(gcc_unused EventLoop &loop,
		     gcc_unused DatabaseListener &listener,
		     const config_param &param, Error &error)
{
	UpnpDatabase *db = new UpnpDatabase();
	if (!db->Configure(param, error)) {
		delete db;
		return nullptr;
	}

	/* libupnp loses its ability to receive multicast messages
	   apparently due to daemonization; using the LazyDatabase
	   wrapper works around this problem */
	return new LazyDatabase(db);
}

bool
UpnpDatabase::Configure(const config_param &, Error &)
{
	return true;
}

bool
UpnpDatabase::Open(Error &error)
{
	if (m_root)
		return true;

	m_lib = LibUPnP::getLibUPnP(error);
	if (!m_lib)
		return false;

	m_superdir = UPnPDeviceDirectory::getTheDir();
	if (!m_superdir || !m_superdir->ok()) {
		error.Set(upnp_domain, "Discovery services startup failed");
		return false;
	}

	m_root = Directory::NewRoot();
	// Wait for device answers. This should be consistent with the value set
	// in the lib (currently 2)
	sleep(2);
	return true;
}

void
UpnpDatabase::Close()
{
	if (m_root)
		delete m_root;
	// TBD decide what we do with the lib and superdir objects
}

void
UpnpDatabase::ReturnSong(Song *song) const
{
	assert(song != nullptr);

	song->Free();
}

/**
 * Transform titles to turn '/' into '_' to make them acceptable path
 * elements. There is a very slight risk of collision in doing
 * this. Twonky returns directory names (titles) like 'Artist/Album'.
 */
gcc_pure
static std::string
titleToPathElt(const std::string &in)
{
	std::string out;
	for (auto it = in.begin(); it != in.end(); it++) {
		if (*it == '/') {
			out += '_';
		} else {
			out += *it;
		}
	}
	return out;
}

// If uri is empty, we use the object's url instead. This happens
// when the target of a Visit() is a song, which  only happens when
// "add"ing AFAIK. Visit() calls us with a null uri so that the url
// appropriate for fetching is used instead.
static Song *
upnpItemToSong(const UPnPDirObject &dirent, const char *uri)
{
	if (*uri == 0)
		uri = dirent.url.c_str();

	Song *s = Song::NewFile(uri, nullptr);

	TagBuilder tag;

	if (dirent.duration > 0)
		tag.SetTime(dirent.duration);

	tag.AddItem(TAG_TITLE, titleToPathElt(dirent.m_title).c_str());

	for (auto i = upnp_tags; i->name != nullptr; ++i) {
		const char *value = dirent.getprop(i->name);
		if (value != nullptr)
			tag.AddItem(i->type, value);
	}

	s->tag = tag.CommitNew();
	return s;
}

// Get song info by path. We can receive either the id path, or the titles
// one
Song *
UpnpDatabase::GetSong(const char *uri, Error &error) const
{
	if (!m_superdir || !m_superdir->ok()) {
		error.Set(upnp_domain,
			  "UpnpDatabase::GetSong() superdir is sick");
		return nullptr;
	}

	Song *song = nullptr;
	auto vpath = stringToTokens(uri, "/", true);
	if (vpath.size() >= 2) {
		ContentDirectoryService server;
		if (!m_superdir->getServer(vpath[0].c_str(), server)) {
			error.Set(upnp_domain, "server not found");
			return nullptr;
		}

		vpath.erase(vpath.begin());
		UPnPDirObject dirent;
		if (vpath[0].compare(rootid)) {
			std::string objid;
			if (!Namei(&server, vpath, objid, dirent, error))
				return nullptr;
		} else {
			if (!ReadNode(&server, vpath.back().c_str(), dirent,
				      error))
				return nullptr;
		}
		song = upnpItemToSong(dirent, "");
	}
	if (song == nullptr)
		error.Format(db_domain, DB_NOT_FOUND, "No such song: %s", uri);

	return song;
}

/**
 * Retrieve the value for an MPD tag from an object entry.
 */
static bool
getTagValue(UPnPDirObject& dirent, TagType tag,
	    std::string &tagvalue)
{
	if (tag == TAG_TITLE) {
		if (!dirent.m_title.empty()) {
			tagvalue = titleToPathElt(dirent.m_title);
			return true;
		}
		return false;
	}

	const char *name = tag_table_lookup(upnp_tags, tag);
	if (name == nullptr)
		return false;

	const char *value = dirent.getprop(name);
	if (value == nullptr)
		return false;

	tagvalue = value;
	return true;
}

/**
 * Double-quote a string, adding internal backslash escaping.
 */
static void
dquote(std::string &out, const char *in)
{
	out.append(1, '"');

	for (; *in != 0; ++in) {
		switch(*in) {
		case '\\':
		case '"':
			out.append(1, '\\');
			out.append(1, *in);
			break;

		default:
			out.append(1, *in);
		}
	}

	out.append(1, '"');
}

// Run an UPnP search, according to MPD parameters. Return results as
// UPnP items
bool
UpnpDatabase::SearchSongs(ContentDirectoryService* server,
			  const char *objid,
			  const DatabaseSelection &selection,
			  UPnPDirContent &dirbuf,
			  Error &error) const
{
	const SongFilter *filter = selection.filter;
	if (selection.filter == nullptr)
		return true;

	std::set<std::string> searchcaps;
	if (!server->getSearchCapabilities(searchcaps, error))
		return false;

	if (searchcaps.empty())
		return true;

	std::string cond;
	for (const auto &item : filter->GetItems()) {
		switch (auto tag = item.GetTag()) {
		case LOCATE_TAG_ANY_TYPE:
			{
				if (!cond.empty()) {
					cond += " and ";
				}
				cond += "(";
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
					dquote(cond, item.GetValue().c_str());
				}
				cond += ")";
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
			dquote(cond, item.GetValue().c_str());
		}
	}

	return server->search(objid, cond.c_str(), dirbuf, error);
}

static bool
visitSong(const UPnPDirObject& meta, const char *path,
	  const DatabaseSelection &selection,
	  VisitSong visit_song, Error& error)
{
	if (!visit_song)
		return true;
	Song *s = upnpItemToSong(meta, path);
	if (!selection.Match(*s))
		return true;
	bool success = visit_song(*s, error);
	s->Free();
	return success;
}

/**
 * Build synthetic path based on object id for search results. The use
 * of "rootid" is arbitrary, any name that is not likely to be a top
 * directory name would fit.
 */
static const std::string
songPath(const std::string &servername,
	 const std::string &objid)
{
	return servername + "/" + rootid + "/" + objid;
}

bool
UpnpDatabase::SearchSongs(ContentDirectoryService* server,
			  const char *objid,
			  const DatabaseSelection &selection,
			  VisitSong visit_song,
			  Error &error) const
{
	UPnPDirContent dirbuf;
	if (!visit_song)
		return true;
	if (!SearchSongs(server, objid, selection, dirbuf, error))
		return false;

	for (const auto &dirent : dirbuf.m_items) {
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
		std::string path = songPath(server->getFriendlyName(),
					    dirent.m_id);
		//BuildPath(server, dirent, path);
		if (!visitSong(dirent, path.c_str(), selection, visit_song,
			       error))
			return false;
	}

	return true;
}

bool
UpnpDatabase::ReadNode(ContentDirectoryService *server,
		       const char *objid, UPnPDirObject &dirent,
		       Error &error) const
{
	UPnPDirContent dirbuf;
	if (!server->getMetadata(objid, dirbuf, error))
		return false;

	if (dirbuf.m_containers.size() == 1) {
		dirent = dirbuf.m_containers[0];
	} else if (dirbuf.m_items.size() == 1) {
		dirent = dirbuf.m_items[0];
	} else {
		error.Format(upnp_domain, "Bad resource");
		return false;
	}

	return true;
}

bool
UpnpDatabase::BuildPath(ContentDirectoryService *server,
			const UPnPDirObject& idirent,
			std::string &path,
			Error &error) const
{
	const char *pid = idirent.m_id.c_str();
	path.clear();
	UPnPDirObject dirent;
	while (strcmp(pid, rootid) != 0) {
		if (!ReadNode(server, pid, dirent, error))
			return false;
		pid = dirent.m_pid.c_str();
		path = titleToPathElt(dirent.m_title) + (path.empty()? "" : "/" + path);
	}
	path = std::string(server->getFriendlyName()) + "/" + path;
	return true;
}

// Take server and internal title pathname and return objid and metadata.
bool
UpnpDatabase::Namei(ContentDirectoryService* server,
		    const std::vector<std::string> &vpath,
		    std::string &oobjid, UPnPDirObject &odirent,
		    Error &error) const
{
	oobjid.clear();
	std::string objid(rootid);

	if (vpath.empty()) {
		// looking for root info
		if (!ReadNode(server, rootid, odirent, error))
			return false;

		oobjid = rootid;
		return true;
	}

	// Walk the path elements, read each directory and try to find the next one
	for (unsigned int i = 0; i < vpath.size(); i++) {
		UPnPDirContent dirbuf;
		if (!server->readDir(objid.c_str(), dirbuf, error))
			return false;

		bool found = false;

		// Look for the name in the sub-container list
		for (auto& dirent : dirbuf.m_containers) {
			if (!vpath[i].compare(titleToPathElt(dirent.m_title.c_str()))) {
				objid = dirent.m_id; // Next readdir target
				found = true;
				if (i == vpath.size() - 1) {
					// The last element in the path was found and it's
					// a container, we're done
					oobjid = objid;
					odirent = dirent;
					return true;
				}
				break;
			}
		}
		if (found)
			continue;

		// Path elt was not a container, look at the items list
		for (auto& dirent : dirbuf.m_items) {
			if (!vpath[i].compare(titleToPathElt(dirent.m_title.c_str()))) {
				// If this is the last path elt, we found the target,
				// else it does not exist
				if (i == vpath.size() - 1) {
					oobjid = objid;
					odirent = dirent;
					return true;
				} else {
					return true;
				}
			}
		}

		// Neither container nor item, we're done.
		if (!found)
			break;
	}

	return true;
}

// vpath is a parsed and writeable version of selection.uri. There is
// really just one path parameter.
bool
UpnpDatabase::VisitServer(ContentDirectoryService* server,
			  const std::vector<std::string> &vpath,
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
	if (!vpath.empty() && !vpath[0].compare(rootid)) {
		if (visit_song) {
			UPnPDirObject dirent;
			if (!ReadNode(server, vpath.back().c_str(), dirent,
				      error))
				return false;

			if (!visitSong(dirent, "", selection,
				       visit_song, error))
				return false;
		}
		return true;
	}

	// Translate the target path into an object id and the associated metadata.
	std::string objid;
	UPnPDirObject tdirent;
	if (!Namei(server, vpath, objid, tdirent, error))
		return false;

	if (objid.empty())
		// Not found, not a fatal error
		return true;

	/* If recursive is set, this is a search... No use sending it
	   if the filter is empty. In this case, we implement limited
	   recursion (1-deep) here, which will handle the "add dir"
	   case. */
	if (selection.recursive && selection.filter)
		return SearchSongs(server, objid.c_str(), selection,
				   visit_song, error);

	if (tdirent.type == UPnPDirObject::Type::ITEM) {
		// Target is a song. Not too sure we ever get there actually, maybe
		// this is always catched by the special uri test above.
		switch (tdirent.item_class) {
		case UPnPDirObject::ItemClass::MUSIC:
			if (visit_song)
				return visitSong(tdirent, "", selection, visit_song,
						 error);
			break;

		case UPnPDirObject::ItemClass::PLAYLIST:
			if (visit_playlist) {
				/* Note: I've yet to see a playlist
				 item (playlists seem to be usually
				 handled as containers, so I'll decide
				 what to do when I see one... */
			}
			break;

		case UPnPDirObject::ItemClass::UNKNOWN:
			break;
		}

		return true;
	}

	/* Target was a a container. Visit it. We could read slices
	   and loop here, but it's not useful as mpd will only return
	   data to the client when we're done anyway. */
	UPnPDirContent dirbuf;
	if (!server->readDir(objid.c_str(), dirbuf, error))
		return false;

	if (visit_directory) {
		for (auto& dirent : dirbuf.m_containers) {
			Directory d((selection.uri + "/" +
				     titleToPathElt(dirent.m_title)).c_str(),
				    m_root);
			if (!visit_directory(d, error))
				return false;
		}
	}

	if (visit_song || visit_playlist) {
		for (const auto &dirent : dirbuf.m_items) {
			switch (dirent.item_class) {
			case UPnPDirObject::ItemClass::MUSIC:
				if (visit_song) {
					/* We identify songs by giving
					   them a special path. The Id
					   is enough to fetch them
					   from the server anyway. */

					std::string p;
					if (!selection.recursive)
						p = selection.uri + "/" +
							titleToPathElt(dirent.m_title);

					if (!visitSong(dirent, p.c_str(),
						       selection, visit_song, error))
						return false;
				}

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
			}
		}
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
	std::vector<ContentDirectoryService> servers;
	if (!m_superdir || !m_superdir->ok() ||
	    !m_superdir->getDirServices(servers)) {
		error.Set(upnp_domain,
			  "UpnpDatabase::Visit() superdir is sick");
		return false;
	}

	auto vpath = stringToTokens(selection.uri, "/", true);
	if (vpath.empty()) {
		if (!selection.recursive) {
			// If the path is empty and recursive is not set, synthetize a
			// pseudo-directory from the list of servers.
			if (visit_directory) {
				for (auto& server : servers) {
					Directory d(server.getFriendlyName(), m_root);
					if (!visit_directory(d, error))
						return false;
				}
			}
		} else {
			// Recursive is set: visit each server
			for (auto& server : servers) {
				if (!VisitServer(&server, std::vector<std::string>(), selection,
						 visit_directory, visit_song, visit_playlist, error))
					return false;
			}
		}
		return true;
	}

	// We do have a path: the first element selects the server
	std::string servername(vpath[0]);
	vpath.erase(vpath.begin());

	ContentDirectoryService* server = 0;
	for (auto& dir : servers) {
		if (!servername.compare(dir.getFriendlyName())) {
			server = &dir;
			break;
		}
	}
	if (server == 0) {
		FormatDebug(db_domain, "UpnpDatabase::Visit: server %s not found\n",
			    vpath[0].c_str());
		return true;
	}
	return VisitServer(server, vpath, selection,
			   visit_directory, visit_song, visit_playlist, error);
}

bool
UpnpDatabase::VisitUniqueTags(const DatabaseSelection &selection,
			      TagType tag,
			      VisitString visit_string,
			      Error &error) const
{
	if (!visit_string)
		return true;

	std::vector<ContentDirectoryService> servers;
	if (!m_superdir || !m_superdir->ok() ||
	    !m_superdir->getDirServices(servers)) {
		error.Set(upnp_domain,
			  "UpnpDatabase::Visit() superdir is sick");
		return false;
	}

	std::set<std::string> values;
	for (auto& server : servers) {
		UPnPDirContent dirbuf;
		if (!SearchSongs(&server, rootid, selection, dirbuf, error))
			return false;

		for (auto &dirent : dirbuf.m_items) {
			std::string tagvalue;
			if (getTagValue(dirent, tag, tagvalue)) {
#if defined(__clang__) || GCC_CHECK_VERSION(4,8)
				values.emplace(std::move(tagvalue));
#else
				values.insert(std::move(tagvalue));
#endif
			}
		}
	}

	for (const auto& value : values)
		if (!visit_string(value.c_str(), error))
			return false;

	return true;
}

bool
UpnpDatabase::GetStats(const DatabaseSelection &,
		       DatabaseStats &stats, Error &) const
{
	/* Note: this gets called before the daemonizing so we can't
	   reallyopen this would be a problem if we had real stats */
	stats.song_count = 0;
	stats.total_duration = 0;
	stats.artist_count = 0;
	stats.album_count = 0;
	return true;
}

const DatabasePlugin upnp_db_plugin = {
	"upnp",
	UpnpDatabase::Create,
};
