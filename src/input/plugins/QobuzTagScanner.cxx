// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "QobuzTagScanner.hxx"
#include "QobuzErrorParser.hxx"
#include "QobuzClient.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"

#include <nlohmann/json.hpp>

using std::string_view_literals::operator""sv;

static std::string
MakeTrackUrl(QobuzClient &client, std::string_view track_id)
{
	return client.MakeUrl("track", "get",
			      {
				      {"track_id", std::string{track_id}},
			      });
}

QobuzTagScanner::QobuzTagScanner(QobuzClient &client,
				 std::string_view track_id,
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

static void
from_json(const nlohmann::json &j, Tag &tag)
{
	TagBuilder b;

	if (auto i = j.find("duration"sv); i != j.end())
		b.SetDuration(SignedSongTime::FromS(i->get<unsigned>()));

	if (auto i = j.find("title"sv); i != j.end())
		b.AddItem(TAG_TITLE, i->get<std::string_view>());

	if (const auto a = j.find("album"sv); a != j.end()) {
		if (auto i = a->find("title"sv); i != a->end())
			b.AddItem(TAG_ALBUM, i->get<std::string_view>());

		if (auto i = a->find("artist"sv); i != a->end())
			b.AddItem(TAG_ALBUM_ARTIST, i->at("name"sv).get<std::string_view>());
	}

	if (auto i = j.find("composer"sv); i != j.end())
		b.AddItem(TAG_COMPOSER, i->at("name"sv).get<std::string_view>());

	if (auto i = j.find("performer"sv); i != j.end())
		b.AddItem(TAG_PERFORMER, i->at("name"sv).get<std::string_view>());

	b.Commit(tag);
}

void
QobuzTagScanner::OnEnd()
{
	const auto &r = GetResponse();
	if (r.status != 200)
		ThrowQobuzError(r);

	if (auto i = r.headers.find("content-type");
	    i == r.headers.end() || i->second.find("/json") == i->second.npos)
		throw std::runtime_error("Not a JSON response from Qobuz");

	handler.OnRemoteTag(nlohmann::json::parse(r.body));
}

void
QobuzTagScanner::OnError(std::exception_ptr e) noexcept
{
	handler.OnRemoteTagError(e);
}
