// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "QobuzTrackRequest.hxx"
#include "QobuzErrorParser.hxx"
#include "QobuzClient.hxx"

#include <fmt/core.h>
#include <nlohmann/json.hpp>

using std::string_view_literals::operator""sv;

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
	request_headers.Append(fmt::format("X-User-Auth-Token:{}"sv,
					   session.user_auth_token).c_str());
	request.GetEasy().SetRequestHeaders(request_headers.Get());
}

QobuzTrackRequest::~QobuzTrackRequest() noexcept
{
	request.StopIndirect();
}

void
QobuzTrackRequest::OnEnd()
{
	const auto &r = GetResponse();
	if (r.status != 200)
		ThrowQobuzError(r);

	if (auto i = r.headers.find("content-type");
	    i == r.headers.end() || i->second.find("/json") == i->second.npos)
		throw std::runtime_error("Not a JSON response from Qobuz");

	handler.OnQobuzTrackSuccess(nlohmann::json::parse(r.body).at("url"sv).get<std::string>());
}

void
QobuzTrackRequest::OnError(std::exception_ptr e) noexcept
{
	handler.OnQobuzTrackError(e);
}
