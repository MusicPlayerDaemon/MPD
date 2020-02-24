/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
#include "QobuzTagScanner.hxx"
#include "QobuzErrorParser.hxx"
#include "QobuzClient.hxx"
#include "lib/yajl/Callbacks.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"

using Wrapper = Yajl::CallbacksWrapper<QobuzTagScanner::ResponseParser>;
static constexpr yajl_callbacks parse_callbacks = {
	nullptr,
	nullptr,
	Wrapper::Integer,
	nullptr,
	nullptr,
	Wrapper::String,
	Wrapper::StartMap,
	Wrapper::MapKey,
	Wrapper::EndMap,
	nullptr,
	nullptr,
};

class QobuzTagScanner::ResponseParser final : public YajlResponseParser {
	enum class State {
		NONE,
		COMPOSER,
		COMPOSER_NAME,
		DURATION,
		TITLE,
		ALBUM,
		ALBUM_TITLE,
		ALBUM_ARTIST,
		ALBUM_ARTIST_NAME,
		ALBUM_IMAGE,
		ALBUM_IMAGE_URL,
		PERFORMER,
		PERFORMER_NAME,
	} state = State::NONE;

	unsigned map_depth = 0;
	unsigned image_type = 0;
	unsigned biggest_image_type = 0;

	TagBuilder tag;

public:
	explicit ResponseParser() noexcept
		:YajlResponseParser(&parse_callbacks, nullptr, this) {}

	Tag GetTag() {
		return tag.Commit();
	}

	/* yajl callbacks */
	bool Integer(long long value) noexcept;
	bool String(StringView value) noexcept;
	bool StartMap() noexcept;
	bool MapKey(StringView value) noexcept;
	bool EndMap() noexcept;
};

static std::string
MakeTrackUrl(QobuzClient &client, const char *track_id)
{
	return client.MakeUrl("track", "get",
			      {
				      {"track_id", track_id},
				      {"user_auth_token", client.GetSession().user_auth_token.c_str()},
			      });
}

QobuzTagScanner::QobuzTagScanner(QobuzClient &client,
				 const char *track_id,
				 RemoteTagHandler &_handler)
	:request(client.GetCurl(),
		 MakeTrackUrl(client, track_id).c_str(),
		 *this),
	 handler(_handler)
{
}

QobuzTagScanner::~QobuzTagScanner() noexcept
{
	request.StopIndirect();
}

std::unique_ptr<CurlResponseParser>
QobuzTagScanner::MakeParser(unsigned status,
			    std::multimap<std::string, std::string> &&headers)
{
	if (status != 200)
		return std::make_unique<QobuzErrorParser>(status, headers);

	auto i = headers.find("content-type");
	if (i == headers.end() || i->second.find("/json") == i->second.npos)
		throw std::runtime_error("Not a JSON response from Qobuz");

	return std::make_unique<ResponseParser>();
}

void
QobuzTagScanner::FinishParser(std::unique_ptr<CurlResponseParser> p)
{
	assert(dynamic_cast<ResponseParser *>(p.get()) != nullptr);
	auto &rp = (ResponseParser &)*p;
	handler.OnRemoteTag(rp.GetTag());
}

void
QobuzTagScanner::OnError(std::exception_ptr e, gcc_unused int code) noexcept
{
	handler.OnRemoteTagError(e);
}

inline bool
QobuzTagScanner::ResponseParser::Integer(long long value) noexcept
{
	switch (state) {
	case State::DURATION:
		if (value > 0)
			tag.SetDuration(SignedSongTime::FromS((unsigned)value));
		break;

	default:
		break;
	}

	return true;
}

inline bool
QobuzTagScanner::ResponseParser::String(StringView value) noexcept
{
	switch (state) {
	case State::TITLE:
		if (map_depth == 1)
			tag.AddItem(TAG_TITLE, value);
		break;

	case State::COMPOSER_NAME:
		if (map_depth == 2)
			tag.AddItem(TAG_COMPOSER, value);
		break;

	case State::ALBUM_TITLE:
		if (map_depth == 2)
			tag.AddItem(TAG_ALBUM, value);
		break;

	case State::ALBUM_ARTIST_NAME:
		if (map_depth == 3)
			tag.AddItem(TAG_ALBUM_ARTIST, value);
		break;

	case State::ALBUM_IMAGE_URL:
		if (map_depth == 3) {
			if (tag.HasType(TAG_ALBUM_URI)) {
				if (biggest_image_type <= image_type) {
					biggest_image_type = image_type;
					tag.RemoveType(TAG_ALBUM_URI);
					tag.AddItem(TAG_ALBUM_URI, value);
				}
			} else {
				biggest_image_type = image_type;
				tag.AddItem(TAG_ALBUM_URI, value);
			}
		}
		break;

	case State::PERFORMER_NAME:
		if (map_depth == 2)
			tag.AddItem(TAG_PERFORMER, value);
		break;

	default:
		break;
	}

	return true;
}

inline bool
QobuzTagScanner::ResponseParser::StartMap() noexcept
{
	++map_depth;
	return true;
}

inline bool
QobuzTagScanner::ResponseParser::MapKey(StringView value) noexcept
{
	switch (map_depth) {
	case 1:
		if (value.Equals("composer"))
			state = State::COMPOSER;
		else if (value.Equals("duration"))
			state = State::DURATION;
		else if (value.Equals("title"))
			state = State::TITLE;
		else if (value.Equals("album"))
			state = State::ALBUM;
		else if (value.Equals("performer"))
			state = State::PERFORMER;
		else
			state = State::NONE;
		break;

	case 2:
		switch (state) {
		case State::NONE:
		case State::DURATION:
		case State::TITLE:
			break;

		case State::COMPOSER:
		case State::COMPOSER_NAME:
			if (value.Equals("name"))
				state = State::COMPOSER_NAME;
			else
				state = State::COMPOSER;
			break;

		case State::ALBUM:
		case State::ALBUM_TITLE:
		case State::ALBUM_ARTIST:
		case State::ALBUM_ARTIST_NAME:
		case State::ALBUM_IMAGE:
		case State::ALBUM_IMAGE_URL:
			if (value.Equals("title"))
				state = State::ALBUM_TITLE;
			else if (value.Equals("artist"))
				state = State::ALBUM_ARTIST;
			else if (value.Equals("image"))
				state = State::ALBUM_IMAGE;
			else
				state = State::ALBUM;
			break;

		case State::PERFORMER:
		case State::PERFORMER_NAME:
			if (value.Equals("name"))
				state = State::PERFORMER_NAME;
			else
				state = State::PERFORMER;
			break;

		default:
			break;
		}
		break;

	case 3:
		switch (state) {
		case State::ALBUM_ARTIST:
		case State::ALBUM_ARTIST_NAME:
			if (value.Equals("name"))
				state = State::ALBUM_ARTIST_NAME;
			else
				state = State::ALBUM_ARTIST;
			break;

		case State::ALBUM_IMAGE:
		case State::ALBUM_IMAGE_URL:
			if (value.Equals("large")) {
				state = State::ALBUM_IMAGE_URL;
				image_type = 4;
			} else if (value.Equals("small")) {
				state = State::ALBUM_IMAGE_URL;
				image_type = 3;
			} else if (value.Equals("back")) {
				state = State::ALBUM_IMAGE_URL;
				image_type = 2;
			} else if (value.Equals("thumbnail")) {
				state = State::ALBUM_IMAGE_URL;
				image_type = 1;
			}
		default:
			break;
		}
		break;
	}

	return true;
}

inline bool
QobuzTagScanner::ResponseParser::EndMap() noexcept
{
	switch (map_depth) {
	case 2:
		state = State::NONE;
		break;

	case 3:
		switch (state) {
		case State::ALBUM_TITLE:
		case State::ALBUM_ARTIST:
		case State::ALBUM_ARTIST_NAME:
			state = State::ALBUM;
			break;

		default:
			break;
		}
		break;
	}

	--map_depth;

	return true;
}
