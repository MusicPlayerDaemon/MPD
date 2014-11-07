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
#include "Directory.hxx"
#include "lib/upnp/Util.hxx"
#include "lib/expat/ExpatParser.hxx"
#include "Tags.hxx"
#include "tag/TagBuilder.hxx"
#include "tag/TagTable.hxx"
#include "util/NumberParser.hxx"

#include <algorithm>
#include <string>

#include <string.h>

UPnPDirContent::~UPnPDirContent()
{
	/* this destructor exists here just so it won't get inlined */
}

gcc_pure gcc_nonnull_all
static bool
CompareStringLiteral(const char *literal, const char *value, size_t length)
{
	return length == strlen(literal) &&
		memcmp(literal, value, length) == 0;
}

gcc_pure
static UPnPDirObject::ItemClass
ParseItemClass(const char *name, size_t length)
{
	if (CompareStringLiteral("object.item.audioItem.musicTrack",
				 name, length))
		return UPnPDirObject::ItemClass::MUSIC;
	else if (CompareStringLiteral("object.item.playlistItem",
				      name, length))
		return UPnPDirObject::ItemClass::PLAYLIST;
	else
		return UPnPDirObject::ItemClass::UNKNOWN;
}

gcc_pure
static SignedSongTime
ParseDuration(const char *duration)
{
	char *endptr;

	unsigned result = ParseUnsigned(duration, &endptr);
	if (endptr == duration || *endptr != ':')
		return SignedSongTime::Negative();

	result *= 60;
	duration = endptr + 1;
	result += ParseUnsigned(duration, &endptr);
	if (endptr == duration || *endptr != ':')
		return SignedSongTime::Negative();

	result *= 60;
	duration = endptr + 1;
	result += ParseUnsigned(duration, &endptr);
	if (endptr == duration || *endptr != 0)
		return SignedSongTime::Negative();

	return SignedSongTime::FromS(result);
}

/**
 * Transform titles to turn '/' into '_' to make them acceptable path
 * elements. There is a very slight risk of collision in doing
 * this. Twonky returns directory names (titles) like 'Artist/Album'.
 */
gcc_pure
static std::string
titleToPathElt(std::string &&s)
{
	std::replace(s.begin(), s.end(), '/', '_');
	return s;
}

/**
 * An XML parser which builds directory contents from DIDL lite input.
 */
class UPnPDirParser final : public CommonExpatParser {
	UPnPDirContent &m_dir;

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

	UPnPDirObject m_tobj;
	TagBuilder tag;

public:
	UPnPDirParser(UPnPDirContent& dir)
		:m_dir(dir),
		 state(NONE),
		 tag_type(TAG_NUM_OF_ITEM_TYPES)
	{
		m_tobj.clear();
	}

protected:
	virtual void StartElement(const XML_Char *name, const XML_Char **attrs)
	{
		if (m_tobj.type != UPnPDirObject::Type::UNKNOWN &&
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
				m_tobj.clear();
				m_tobj.type = UPnPDirObject::Type::CONTAINER;

				const char *id = GetAttribute(attrs, "id");
				if (id != nullptr)
					m_tobj.m_id = id;

				const char *pid = GetAttribute(attrs, "parentID");
				if (pid != nullptr)
					m_tobj.m_pid = pid;
			}
			break;

		case 'i':
			if (!strcmp(name, "item")) {
				m_tobj.clear();
				m_tobj.type = UPnPDirObject::Type::ITEM;

				const char *id = GetAttribute(attrs, "id");
				if (id != nullptr)
					m_tobj.m_id = id;

				const char *pid = GetAttribute(attrs, "parentID");
				if (pid != nullptr)
					m_tobj.m_pid = pid;
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

	bool checkobjok() {
		if (m_tobj.m_id.empty() || m_tobj.m_pid.empty() ||
		    m_tobj.name.empty() ||
		    (m_tobj.type == UPnPDirObject::Type::ITEM &&
		     m_tobj.item_class == UPnPDirObject::ItemClass::UNKNOWN))
			return false;

		return true;
	}

	virtual void EndElement(const XML_Char *name)
	{
		if (tag_type != TAG_NUM_OF_ITEM_TYPES) {
			assert(m_tobj.type != UPnPDirObject::Type::UNKNOWN);

			tag.AddItem(tag_type, value.c_str());

			if (tag_type == TAG_TITLE)
				m_tobj.name = titleToPathElt(std::move(value));

			value.clear();
			tag_type = TAG_NUM_OF_ITEM_TYPES;
			return;
		}

		if ((!strcmp(name, "container") || !strcmp(name, "item")) &&
		    checkobjok()) {
			tag.Commit(m_tobj.tag);
			m_dir.objects.emplace_back(std::move(m_tobj));
		}

		state = NONE;
	}

	virtual void CharacterData(const XML_Char *s, int len)
	{
		if (tag_type != TAG_NUM_OF_ITEM_TYPES) {
			assert(m_tobj.type != UPnPDirObject::Type::UNKNOWN);

			value.append(s, len);
			return;
		}

		switch (state) {
		case NONE:
			break;

		case RES:
			m_tobj.url.assign(s, len);
			break;

		case CLASS:
			m_tobj.item_class = ParseItemClass(s, len);
			break;
		}
	}
};

bool
UPnPDirContent::parse(const char *input, Error &error)
{
	UPnPDirParser parser(*this);
	return parser.Parse(input, strlen(input), true, error);
}
