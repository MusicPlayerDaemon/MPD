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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "Object.hxx"
#include "util/Macros.hxx"
#include "util/ASCII.hxx"
#include "util/StringView.hxx"
#include "util/NumberParser.hxx"
#include "util/UriUtil.hxx"
#include "util/StringUtil.hxx"
#include "util/FormatString.hxx"
#include "util/AllocatedString.hxx"
#include "db/LightSong.hxx"
#include "db/Selection.hxx"
#include "db/LightDirectory.hxx"
#include "db/DatabaseError.hxx"
#include "fs/Traits.hxx"
#include "Tags.hxx"
#include "tag/Builder.hxx"
#include "tag/Table.hxx"
#include "lib/upnp/Util.hxx"
#include "lib/expat/ExpatParser.hxx"
#include "lib/upnp/ContentDirectoryService.hxx"

UPnPDirObject::~UPnPDirObject() noexcept
{
	/* this destructor exists here just so it won't get inlined */
}

gcc_pure
static UPnPDirObject::ItemClass
ParseItemClass(StringView name) noexcept
{
	if (name.Equals("object.item.audioItem.musicTrack"))
		return UPnPDirObject::ItemClass::MUSIC;
	else if (name.Equals("object.item.playlistItem"))
		return UPnPDirObject::ItemClass::PLAYLIST;
	else if(name.Equals("object.container.album.musicAlbum"))
		return UPnPDirObject::ItemClass::ALBUM;
	else if(name.Equals("object.container.genre.musicGenre"))
		return UPnPDirObject::ItemClass::GENRE;
	else if(name.Equals("object.container.person.musicArtist"))
		return UPnPDirObject::ItemClass::ARTIST;
	else if(name.Equals("object.container.storageFolder"))
		return UPnPDirObject::ItemClass::FOLDER;
	else {
		return UPnPDirObject::ItemClass::UNKNOWN;
	}
}

gcc_pure
static SignedSongTime
ParseDuration(const char *duration) noexcept
{
	char *endptr;

	int hours = ParseInt(duration, &endptr);
	if (endptr == duration || *endptr != ':')
		return SignedSongTime::Negative();

	duration = endptr + 1;
	unsigned minutes = ParseUnsigned(duration, &endptr);
	if (endptr == duration || *endptr != ':')
		return SignedSongTime::Negative();

	duration = endptr + 1;
	double seconds = ParseDouble(duration, &endptr);
	if (endptr == duration || *endptr != 0 || seconds < 0.0)
		return SignedSongTime::Negative();

	return SignedSongTime::FromS((((hours * 60) + minutes) * 60) + seconds);
}

/**
 * Transform titles to turn '/' into '_' to make them acceptable path
 * elements. There is a very slight risk of collision in doing
 * this. Twonky returns directory names (titles) like 'Artist/Album'.
 */
gcc_pure
static std::string &&
TitleToPathSegment(std::string &&s)
{
	std::replace(s.begin(), s.end(), '/', '_');
	return std::move(s);
}

static const char *jpeg_type_tbl[] = {
	"JPEG_TN",
	"JPEG_SM",
	"JPEG_LRG",
};

/**
 * An XML parser which builds directory contents from DIDL lite input.
 */
class UPnPDirParser final : public CommonExpatParser {
	UPnPDirObject &directory;

	enum {
		NONE,
		RES,
		CLASS,
	} state;

	typedef enum {
		JPEG_TN,
		JPEG_SM,
		JPEG_LRG,
	} jpeg_type_t;

	/**
	 * If not equal to #TAG_NUM_OF_ITEM_TYPES, then we're
	 * currently reading an element containing a tag value.  The
	 * value is being constructed in #value.
	 */
	TagType tag_type;

	/**
	 * The text inside the current element.
	 */
	std::string value;

	UPnPDirObject object;
	TagBuilder tag;

	std::string suffix;

	jpeg_type_t jpeg_type;

public:
	UPnPDirParser(UPnPDirObject &_directory)
		:directory(_directory),
		 state(NONE),
		 tag_type(TAG_NUM_OF_ITEM_TYPES),
		 jpeg_type(JPEG_TN)
	{
		object.Clear();
	}

protected:
	virtual void StartElement(const XML_Char *name, const XML_Char **attrs)
	{
		if (object.type != UPnPDirObject::Type::UNKNOWN &&
		    tag_type == TAG_NUM_OF_ITEM_TYPES) {
			tag_type = tag_table_lookup(upnp_tags, name);
			if (tag_type == TAG_ALBUM_URI) {
				const char *jpeg_type_str = GetAttribute(attrs, "dlna:profileID");
				if (jpeg_type_str != nullptr) {
					for (unsigned i=0;i<ARRAY_SIZE(jpeg_type_tbl);i++) {
						if (StringEqualsCaseASCII(jpeg_type_str, jpeg_type_tbl[i])) {
							jpeg_type = (jpeg_type_t)i;
							break;
						}
					}
				}
			}
			if (tag_type != TAG_NUM_OF_ITEM_TYPES)
				return;
		} else {
			assert(tag_type == TAG_NUM_OF_ITEM_TYPES);
		}

		switch (name[0]) {
		case 'c':
			if (!strcmp(name, "container")) {
				object.Clear();
				object.type = UPnPDirObject::Type::CONTAINER;

				const char *id = GetAttribute(attrs, "id");
				if (id != nullptr)
					object.id = id;

				const char *pid = GetAttribute(attrs, "parentID");
				if (pid != nullptr)
					object.parent_id = pid;
			}
			break;

		case 'i':
			if (!strcmp(name, "item")) {
				object.Clear();
				object.type = UPnPDirObject::Type::ITEM;

				const char *id = GetAttribute(attrs, "id");
				if (id != nullptr)
					object.id = id;

				const char *pid = GetAttribute(attrs, "parentID");
				if (pid != nullptr)
					object.parent_id = pid;
			}
			break;

		case 'r':
			if (!strcmp(name, "res")) {
				// <res protocolInfo="http-get:*:audio/mpeg:*" size="5171496"
				// bitrate="24576" duration="00:03:35" sampleFrequency="44100"
				// nrAudioChannels="2">
				const char *duration =
				GetAttribute(attrs, "duration");
				if (duration != nullptr)
					tag.SetDuration(ParseDuration(duration));

				state = RES;
				if (object.url.length() > 0) {
					state = NONE;
				}

				if (suffix.empty()) {
					const char *protocolInfo  = GetAttribute(attrs, "protocolInfo");
					if (protocolInfo != nullptr) {
						std::string str(protocolInfo);
						auto p1 = str.find("http-get:*:");
						if (p1 != std::string::npos) {
							p1 += 11;
							auto p2 = str.find(":", p1);
							if (p2 != std::string::npos) {
								std::string mime_type = str.substr(p1, p2-p1);
								const char *s = mime_table_lookup(mime_types, mime_type.c_str());
								if (s != nullptr) {
									suffix = s;
								}
							}
						}
					}
				}
			}
			break;

		case 'u':
			if (strcmp(name, "upnp:class") == 0)
				state = CLASS;
		}
	}

	virtual void EndElement(const XML_Char *name)
	{
		if (tag_type != TAG_NUM_OF_ITEM_TYPES) {
			assert(object.type != UPnPDirObject::Type::UNKNOWN);

			if (tag_type == TAG_ALBUM_URI) {
				if (jpeg_type > JPEG_TN) {
					tag.RemoveType(TAG_ALBUM_URI);
				} else if (tag.HasType(TAG_ALBUM_URI)){
					value.clear();
					tag_type = TAG_NUM_OF_ITEM_TYPES;
					jpeg_type = JPEG_TN;
					return;
				}
				jpeg_type = JPEG_TN;
			}
			tag.AddItem(tag_type, value.c_str());

			if (tag_type == TAG_TITLE)
				object.name = TitleToPathSegment(std::move(value));

			value.clear();
			tag_type = TAG_NUM_OF_ITEM_TYPES;
			return;
		}

		if ((!strcmp(name, "container") || !strcmp(name, "item")) &&
		    object.Check()) {
			if (!object.url.empty()) {
				UriSuffixBuffer suffix_buffer;
				const char *su = uri_get_suffix(object.url.c_str(), suffix_buffer);
				if (su != nullptr) {
					char ss[64];
					ToUpperASCII(ss, su, sizeof(ss));
					suffix = ss;
				}
			}
			if (!suffix.empty()) {
				tag.AddItem(TAG_SUFFIX, suffix.c_str());
				suffix.clear();
			}
			tag.Commit(object.tag);
			directory.childs.emplace_back(std::move(object));
		}

		state = NONE;
	}

	virtual void CharacterData(const XML_Char *s, int len)
	{
		if (tag_type != TAG_NUM_OF_ITEM_TYPES) {
			assert(object.type != UPnPDirObject::Type::UNKNOWN);

			value.append(s, len);
			return;
		}

		switch (state) {
		case NONE:
			break;

		case RES:
			object.url.append(s, len);
			break;

		case CLASS:
			object.item_class = ParseItemClass(StringView(s, len));
			break;
		}
	}
};

void
UPnPDirObject::Parse(const char *input)
{
	UPnPDirParser parser(*this);
	parser.Parse(input, strlen(input), true);
}

UPnPDirObject *
UPnPDirObject::LookupDirectory(std::list<std::string> vpath)
{
	if (vpath.empty()) {
		return this;
	}
	for (auto &c : childs) {
		if (c.type == Type::CONTAINER &&
			c.name == vpath.front()) {
			vpath.pop_front();
			return c.LookupDirectory(vpath);
		}
	}

	return nullptr;
}

UPnPDirObject *
UPnPDirObject::LookupSong(std::list<std::string> vpath)
{
	if (vpath.empty()) {
		return nullptr;
	}
	std::string fn = vpath.back();
	vpath.pop_back();
	UPnPDirObject *p = LookupDirectory(vpath);
	if (p == nullptr) {
		return nullptr;
	}
	for (auto &c : p->childs) {
		if (c.item_class == ItemClass::MUSIC
			&& c.name == fn) {
			return &c;
		}
	}

	return nullptr;
}

void
UPnPDirObject::visitSong(const char *path,
	  const DatabaseSelection &selection,
	  VisitSong visit_song) const
{
	if (!visit_song)
		return;

	LightSong song;
	song.directory = nullptr;
	song.uri = path;
	song.real_uri = url.c_str();
	song.tag = &tag;
	song.mtime = std::chrono::system_clock::time_point::min();
	song.start_time = song.end_time = SongTime::zero();

	if (selection.Match(song))
		visit_song(song);
}

void
UPnPDirObject::VisitItem(const char *uri,
	  const DatabaseSelection &selection,
	  VisitSong visit_song, VisitPlaylist visit_playlist) const
{
	assert(type == UPnPDirObject::Type::ITEM);

	switch (item_class) {
	case UPnPDirObject::ItemClass::MUSIC:
		if (visit_song)
			visitSong(uri, selection, visit_song);

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

void
UPnPDirObject::VisitObject(const char *uri,
	    const DatabaseSelection &selection,
	    VisitDirectory visit_directory,
	    VisitSong visit_song,
	    VisitPlaylist visit_playlist) const
{
	switch (type) {
	case UPnPDirObject::Type::UNKNOWN:
		assert(false);
		gcc_unreachable();

	case UPnPDirObject::Type::CONTAINER:
		if (visit_directory)
			visit_directory(LightDirectory(uri, std::chrono::system_clock::time_point::min(), total));
		break;

	case UPnPDirObject::Type::ITEM:
		VisitItem(uri, selection, visit_song, visit_playlist);
		break;
	}
}

void
UPnPDirObject::Walk(const char *base_uri,
		const DatabaseSelection &selection,
		VisitDirectory visit_directory, VisitSong visit_song,
		VisitPlaylist visit_playlist) const
{
	assert(!selection.uri.empty());

	for (unsigned i=selection.window_start, end=std::min((unsigned)childs.size(), selection.window_end);
		i<end; i++) {
		const auto &c = childs[i];
		const std::string uri = PathTraitsUTF8::Build(base_uri,
							      c.name.c_str());
		c.VisitObject(uri.c_str(), selection, visit_directory, visit_song, visit_playlist);
		if (selection.recursive
			&& c.type == Type::CONTAINER) {
			c.Walk(uri.c_str(), selection, visit_directory, visit_song, visit_playlist);
		}
	}
}

void
UPnPDirObject::Update(ContentDirectoryService &server,
			UpnpClient_Handle handle,
			unsigned window_end)
{
	unsigned offset = childs.size();
	unsigned count = 0;
	unsigned end = window_end;

	if (offset < end || end == 0) {
		do {
			UPnPDirObject dirbuf;
			server.readDirSlice(handle, id.c_str(), offset, server.m_rdreqcnt, dirbuf,
					  count, total);
			offset += count;
			for (auto &i : dirbuf.childs) {
				childs.emplace_back(std::move(i));
			}
		} while (count > 0 && offset < total && offset < end);
	}
}

void
UPnPDirObject::Update(ContentDirectoryService &server,
			UpnpClient_Handle handle,
			std::list<std::string> vpath,
			unsigned window_end)
{
	unsigned pos = 0;
	std::string obj;

	if (vpath.empty()) {
		Update(server, handle, window_end);
		return;
	} else {
		obj = vpath.front();
		vpath.pop_front();
	}

	do {
		pos += server.m_rdreqcnt;
		Update(server, handle, pos);
		UPnPDirObject *cursor = FindDirectory(obj.c_str());
		if (cursor != nullptr) {
			cursor->Update(server, handle, vpath, window_end);
			return;
		}
	} while (pos < total);

	throw DatabaseError(DatabaseErrorCode::NOT_FOUND,
				    FormatString("No such folder: %s", obj.c_str()).c_str());
}

