// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "QobuzTrackRequest.hxx"
#include "QobuzErrorParser.hxx"
#include "QobuzClient.hxx"
#include "lib/yajl/Callbacks.hxx"

using std::string_view_literals::operator""sv;

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
	bool String(std::string_view value) noexcept;
	bool MapKey(std::string_view value) noexcept;
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
QobuzTrackRequest::ResponseParser::String(std::string_view value) noexcept
{
	switch (state) {
	case State::NONE:
		break;

	case State::URL:
		url = value;
		break;
	}

	return true;
}

inline bool
QobuzTrackRequest::ResponseParser::MapKey(std::string_view value) noexcept
{
	if (value == "url"sv)
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
