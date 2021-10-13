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

#include "Directory.hxx"
#include "lib/expat/ExpatParser.hxx"
#include "Tags.hxx"
#include "tag/Builder.hxx"
#include "tag/Table.hxx"
#include "util/NumberParser.hxx"
#include "util/StringView.hxx"

#include <algorithm>
#include <string>

#include <string.h>

/* this destructor exists here just so it won't get inlined */
UPnPDirContent::~UPnPDirContent() = default;

[[gnu::pure]]
static UPnPDirObject::ItemClass
ParseItemClass(StringView name) noexcept
{
	if (name.Equals("object.item.audioItem.musicTrack"))
		return UPnPDirObject::ItemClass::MUSIC;
	else if (name.Equals("object.item.playlistItem"))
		return UPnPDirObject::ItemClass::PLAYLIST;
	else
		return UPnPDirObject::ItemClass::UNKNOWN;
}

[[gnu::pure]]
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
[[gnu::pure]]
static std::string &&
TitleToPathSegment(std::string &&s) noexcept
{
	std::replace(s.begin(), s.end(), '/', '_');
	return std::move(s);
}

/**
 * An XML parser which builds directory contents from DIDL lite input.
 */
class UPnPDirParser final : public CommonExpatParser {
	UPnPDirContent &directory;

	enum {
		NONE,
		RES,
		CLASS,
	} state;

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

public:
	explicit UPnPDirParser(UPnPDirContent &_directory)
		:directory(_directory),
		 state(NONE),
		 tag_type(TAG_NUM_OF_ITEM_TYPES)
	{
		object.Clear();
	}

protected:
	void StartElement(const XML_Char *name, const XML_Char **attrs) override
	{
		if (object.type != UPnPDirObject::Type::UNKNOWN &&
		    tag_type == TAG_NUM_OF_ITEM_TYPES) {
			tag_type = tag_table_lookup(upnp_tags, name);
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
			}

			break;

		case 'u':
			if (strcmp(name, "upnp:class") == 0)
				state = CLASS;
		}
	}

	void EndElement(const XML_Char *name) override
	{
		if (tag_type != TAG_NUM_OF_ITEM_TYPES) {
			assert(object.type != UPnPDirObject::Type::UNKNOWN);

			tag.AddItem(tag_type, value.c_str());

			if (tag_type == TAG_TITLE)
				object.name = TitleToPathSegment(std::move(value));

			value = {};
			tag_type = TAG_NUM_OF_ITEM_TYPES;
			return;
		}

		if ((!strcmp(name, "container") || !strcmp(name, "item")) &&
		    object.Check()) {
			tag.Commit(object.tag);
			directory.objects.emplace_back(std::move(object));
		}

		state = NONE;
	}

	void CharacterData(const XML_Char *s, int len) override
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
			object.url.assign(s, len);
			break;

		case CLASS:
			object.item_class = ParseItemClass(StringView(s, len));
			break;
		}
	}
};

void
UPnPDirContent::Parse(const char *input)
{
	UPnPDirParser parser(*this);
	parser.Parse(input, strlen(input), true);
}
