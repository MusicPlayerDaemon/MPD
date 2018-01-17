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
#include "QobuzTrackRequest.hxx"
#include "QobuzClient.hxx"
#include "lib/yajl/Callbacks.hxx"
#include "util/RuntimeError.hxx"

using Wrapper = Yajl::CallbacksWrapper<QobuzTrackRequest>;
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

static std::string
MakeTrackUrl(QobuzClient &client, const char *track_id)
{
	return client.MakeSignedUrl("track", "getFileUrl",
				    {
					    {"track_id", track_id},
					    {"format_id", "5"},
				    });
}

QobuzTrackRequest::QobuzTrackRequest(QobuzClient &client,
				     const QobuzSession &session,
				     const char *track_id,
				     QobuzTrackHandler &_handler) noexcept
	:request(client.GetCurl(),
		 MakeTrackUrl(client, track_id).c_str(),
		 *this),
	 parser(&parse_callbacks, nullptr, this),
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

void
QobuzTrackRequest::OnHeaders(unsigned status,
			     std::multimap<std::string, std::string> &&headers)
{
	if (status != 200)
		throw FormatRuntimeError("Status %u from Qobuz", status);

	auto i = headers.find("content-type");
	if (i == headers.end() || i->second.find("/json") == i->second.npos)
		throw std::runtime_error("Not a JSON response from Qobuz");
}

void
QobuzTrackRequest::OnData(ConstBuffer<void> data)
{
	parser.Parse((const unsigned char *)data.data, data.size);
}

void
QobuzTrackRequest::OnEnd()
{
	parser.CompleteParse();

	if (url.empty())
		throw std::runtime_error("No url in track response");

	handler.OnQobuzTrackSuccess(std::move(url));
}

void
QobuzTrackRequest::OnError(std::exception_ptr e) noexcept
{
	handler.OnQobuzTrackError(e);
}

inline bool
QobuzTrackRequest::String(StringView value) noexcept
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
QobuzTrackRequest::MapKey(StringView value) noexcept
{
	if (value.Equals("url"))
		state = State::URL;
	else
		state = State::NONE;

	return true;
}

inline bool
QobuzTrackRequest::EndMap() noexcept
{
	state = State::NONE;

	return true;
}
