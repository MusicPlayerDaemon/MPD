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

#include "QobuzTrackRequest.hxx"
#include "QobuzErrorParser.hxx"
#include "QobuzClient.hxx"
#include "lib/yajl/Callbacks.hxx"

using Wrapper = Yajl::CallbacksWrapper<QobuzTrackRequest::ResponseParser>;
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

class QobuzTrackRequest::ResponseParser final : public YajlResponseParser {
	enum class State {
		NONE,
		URL,
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
MakeTrackUrl(QobuzClient &client, const char *track_id)
{
	return client.MakeSignedUrl("track", "getFileUrl",
				    {
					    {"track_id", track_id},
					    {"format_id", client.GetFormatId()},
				    });
}

QobuzTrackRequest::QobuzTrackRequest(QobuzClient &client,
				     const QobuzSession &session,
				     const char *track_id,
				     QobuzTrackHandler &_handler)
	:request(client.GetCurl(),
		 MakeTrackUrl(client, track_id).c_str(),
		 *this),
	 handler(_handler)
{
	request_headers.Append(("X-User-Auth-Token:"
				+ session.user_auth_token).c_str());
	request.SetOption(CURLOPT_HTTPHEADER, request_headers.Get());
}

QobuzTrackRequest::~QobuzTrackRequest() noexcept
{
	request.StopIndirect();
}

std::unique_ptr<CurlResponseParser>
QobuzTrackRequest::MakeParser(unsigned status,
			      Curl::Headers &&headers)
{
	if (status != 200)
		return std::make_unique<QobuzErrorParser>(status, headers);

	auto i = headers.find("content-type");
	if (i == headers.end() || i->second.find("/json") == i->second.npos)
		throw std::runtime_error("Not a JSON response from Qobuz");

	return std::make_unique<ResponseParser>();
}

void
QobuzTrackRequest::FinishParser(std::unique_ptr<CurlResponseParser> p)
{
	assert(dynamic_cast<ResponseParser *>(p.get()) != nullptr);
	auto &rp = (ResponseParser &)*p;
	handler.OnQobuzTrackSuccess(rp.GetUrl());
}

void
QobuzTrackRequest::OnError(std::exception_ptr e) noexcept
{
	handler.OnQobuzTrackError(e);
}

inline bool
QobuzTrackRequest::ResponseParser::String(StringView value) noexcept
{
	switch (state) {
	case State::NONE:
		break;

	case State::URL:
		url.assign(value.data, value.size);
		break;
	}

	return true;
}

inline bool
QobuzTrackRequest::ResponseParser::MapKey(StringView value) noexcept
{
	if (value.Equals("url"))
		state = State::URL;
	else
		state = State::NONE;

	return true;
}

inline bool
QobuzTrackRequest::ResponseParser::EndMap() noexcept
{
	state = State::NONE;

	return true;
}
