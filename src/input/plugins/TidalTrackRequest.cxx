/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "TidalTrackRequest.hxx"
#include "TidalErrorParser.hxx"
#include "lib/yajl/Callbacks.hxx"

using Wrapper = Yajl::CallbacksWrapper<TidalTrackRequest::ResponseParser>;
static constexpr yajl_callbacks parse_callbacks = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	Wrapper::String,
	nullptr,
	Wrapper::MapKey,
	Wrapper::EndMap,
	nullptr,
	nullptr,
};

class TidalTrackRequest::ResponseParser final : public YajlResponseParser {
	enum class State {
		NONE,
		URLS,
	} state = State::NONE;

	std::string url;

public:
	explicit ResponseParser() noexcept
		:YajlResponseParser(&parse_callbacks, nullptr, this) {}

	std::string &&GetUrl() {
		if (url.empty())
			throw std::runtime_error("No url in track response");

		return std::move(url);
	}

	/* yajl callbacks */
	bool String(StringView value) noexcept;
	bool MapKey(StringView value) noexcept;
	bool EndMap() noexcept;
};

static std::string
MakeTrackUrl(const char *base_url, const char *track_id,
	     const char *audioquality) noexcept
{
	return std::string(base_url)
		+ "/tracks/"
		+ track_id
		+ "/urlpostpaywall?assetpresentation=FULL&audioquality="
		+ audioquality + "&urlusagemode=STREAM";
}

TidalTrackRequest::TidalTrackRequest(CurlGlobal &curl,
				     const char *base_url, const char *token,
				     const char *session,
				     const char *track_id,
				     const char *audioquality,
				     TidalTrackHandler &_handler)
	:request(curl, MakeTrackUrl(base_url, track_id, audioquality).c_str(),
		 *this),
	 handler(_handler)
{
	request_headers.Append((std::string("X-Tidal-Token:")
				+ token).c_str());
	request_headers.Append((std::string("X-Tidal-SessionId:")
				+ session).c_str());
	request.SetOption(CURLOPT_HTTPHEADER, request_headers.Get());
}

TidalTrackRequest::~TidalTrackRequest() noexcept
{
	request.StopIndirect();
}

std::unique_ptr<CurlResponseParser>
TidalTrackRequest::MakeParser(unsigned status,
			      std::multimap<std::string, std::string> &&headers)
{
	if (status != 200)
		return std::make_unique<TidalErrorParser>(status, headers);

	auto i = headers.find("content-type");
	if (i == headers.end() || i->second.find("/json") == i->second.npos)
		throw std::runtime_error("Not a JSON response from Tidal");

	return std::make_unique<ResponseParser>();
}

void
TidalTrackRequest::FinishParser(std::unique_ptr<CurlResponseParser> p)
{
	assert(dynamic_cast<ResponseParser *>(p.get()) != nullptr);
	auto &rp = (ResponseParser &)*p;
	handler.OnTidalTrackSuccess(rp.GetUrl());
}

void
TidalTrackRequest::OnError(std::exception_ptr e) noexcept
{
	handler.OnTidalTrackError(e);
}

inline bool
TidalTrackRequest::ResponseParser::String(StringView value) noexcept
{
	switch (state) {
	case State::NONE:
		break;

	case State::URLS:
		if (url.empty())
			url.assign(value.data, value.size);
		break;
	}

	return true;
}

inline bool
TidalTrackRequest::ResponseParser::MapKey(StringView value) noexcept
{
	if (value.Equals("urls"))
		state = State::URLS;
	else
		state = State::NONE;

	return true;
}

inline bool
TidalTrackRequest::ResponseParser::EndMap() noexcept
{
	state = State::NONE;

	return true;
}
