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
#include "QobuzInputPlugin.hxx"
#include "QobuzClient.hxx"
#include "QobuzTrackRequest.hxx"
#include "QobuzTagScanner.hxx"
#include "CurlInputPlugin.hxx"
#include "PluginUnavailable.hxx"
#include "input/ProxyInputStream.hxx"
#include "input/FailingInputStream.hxx"
#include "input/InputPlugin.hxx"
#include "config/Block.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "util/StringCompare.hxx"
#include "util/Domain.hxx"
#include "tag/Type.h"
#include "Instance.hxx"
#include "Log.hxx"
#include "QobuzRequest.hxx"
#include "QobuzResponse.hxx"
#include "QobuzModel.hxx"
#include "QobuzEvent.hxx"
#include "external/jaijson/Serializer.hxx"

#include <stdexcept>
#include <memory>

#include <time.h>
#include <assert.h>

static QobuzClient *qobuz_client;
static const Domain domain("qobuz");
extern Instance *instance;

QobuzSession &GetQobuzSession()
{
	assert(qobuz_client);

	return qobuz_client->GetSession();
}

QobuzClient &GetQobuzClient()
{
	assert(qobuz_client);

	return *qobuz_client;
}

static std::string
MakeTrackUrl(QobuzClient &client, const char *track_id)
{
	auto str = client.MakeSignedUrl("track", "getFileUrl",
				    {
					    {"track_id", track_id},
					    {"format_id", client.GetFormatId()},
				    });
	FormatDebug(domain, "request: %s", str.c_str());
	return str;
}

class QobuzInputStream final
	: public ProxyInputStream, public QobuzHandler {

	const std::string track_id;
	StreamTrack stream_track;
	QobuzResponse response;

	std::unique_ptr<QobuzRequest<StreamTrack>> track_request;
	std::unique_ptr<QobuzRequest<QobuzResponse>> report_stream_start;

	Cond report_cond;
	std::exception_ptr error;

public:
	QobuzInputStream(const char *_uri, const char *_track_id,
			 Mutex &_mutex, Cond &_cond) noexcept
		: ProxyInputStream(_uri, _mutex, _cond)
		, track_id(_track_id)
	{
		const std::lock_guard<Mutex> protect(mutex);
		const auto session = qobuz_client->GetSession();

		track_request = std::make_unique<QobuzRequest<StreamTrack>>(*qobuz_client,
								    stream_track,
								    MakeTrackUrl(*qobuz_client, track_id.c_str()).c_str(),
								    *this);
		track_request->Start();
	}

	~QobuzInputStream();

	void Check() override {
		if (error)
			std::rethrow_exception(error);
	}

private:
	void Failed(std::exception_ptr e) {
		SetInput(std::make_unique<FailingInputStream>(GetURI(), e,
							      mutex, cond));
	}

private:
	/* virtual methods from QobuzHandler */
	void OnQobuzSuccess() noexcept override;
	void OnQobuzError(std::exception_ptr e) noexcept override;
};

static QobuzEvent
MakeQobuzEvent(QobuzClient &client, const StreamTrack &track)
{
	auto &session = client.GetSession();
	QobuzEvent event;
	event.user_id = std::to_string(session.user_id);
	event.date = time(nullptr);
	event.device_id = session.device_id;
	event.track_id = std::to_string(track.track_id);
	event.credential_id = std::to_string(session.credential_id);
	event.format_id = track.format_id;
	auto it = std::find(session.user_purchases_track_ids.begin(),
		session.user_purchases_track_ids.end(),
		track.track_id);
	event.purchase = it != session.user_purchases_track_ids.end();

	return event;
}

static bool
CheckEvent(QobuzClient &client, const StreamTrack &track)
{
	auto &session = client.GetSession();

	return (session.user_id > 0 &&
		session.device_id > 0 &&
		session.credential_id > 0 &&
		track.track_id > 0);
}

QobuzInputStream::~QobuzInputStream()
{
	if (CheckEvent(*qobuz_client, stream_track)) {
		auto event = MakeQobuzEvent(*qobuz_client, stream_track);
		event.duration = stream_track.duration;
		auto event_str = str(event);
		event_str.insert(0, 1, '[');
		event_str.push_back(']');
		auto report_url = qobuz_client->MakeSignedUrl("track", "reportStreamingEnd",
				    {
				        {"user_auth_token", qobuz_client->GetSession().user_auth_token.c_str()},
				        {"events", event_str.c_str()},
				    });

		auto report_stream_stop = std::make_unique<QobuzRequest<QobuzResponse>>(*qobuz_client,
			response,
			report_url.c_str(),
			*this);
		report_stream_stop->SetPost();
		report_stream_stop->Start();

		const std::lock_guard<Mutex> protect(mutex);
		report_cond.wait(mutex);
	}
}

void
QobuzInputStream::OnQobuzSuccess() noexcept
{
	const std::lock_guard<Mutex> protect(mutex);
	if (!track_request) {
		report_stream_start.reset();
		report_cond.signal();
		return;
	}
	track_request.reset();
	FormatDebug(domain, "real_url: %s\nmime_type: %s", stream_track.url.c_str(), stream_track.mime_type.c_str());

	try {
		SetInput(OpenCurlInputStream(stream_track.url.c_str(), {},
					     mutex, cond));

		if (CheckEvent(*qobuz_client, stream_track)) {
			auto event = MakeQobuzEvent(*qobuz_client, stream_track);
			auto event_str = str(event);
			event_str.insert(0, 1, '[');
			event_str.push_back(']');
			auto report_url = qobuz_client->MakeSignedUrl("track", "reportStreamingStart",
					    {
					        {"user_auth_token", qobuz_client->GetSession().user_auth_token.c_str()},
					        {"events", event_str.c_str()},
					    });

			report_stream_start = std::make_unique<QobuzRequest<QobuzResponse>>(*qobuz_client,
				response,
				report_url.c_str(),
				*this);
			report_stream_start->SetPost();
			report_stream_start->Start();
		}
	} catch (...) {
		Failed(std::current_exception());
	}
}

void
QobuzInputStream::OnQobuzError(std::exception_ptr e) noexcept
{
	const std::lock_guard<Mutex> protect(mutex);
	if (!track_request) {
		report_stream_start.reset();
		report_cond.signal();
		return;
	}
	track_request.reset();

	Failed(e);
}

static void
InitQobuzInput(EventLoop &event_loop, const ConfigBlock &block)
{
	const char *base_url = block.GetBlockValue("base_url",
						   "http://www.qobuz.com/api.json/0.2/");
	qobuz_client = new QobuzClient(event_loop, base_url);
}

static void
FinishQobuzInput()
{
	delete qobuz_client;
}

gcc_pure
static const char *
ExtractQobuzTrackId(const char *uri)
{
	// TODO: what's the standard "qobuz://" URI syntax?
	const char *track_id = StringAfterPrefix(uri, "qobuz://track/");
	if (track_id == nullptr)
		return nullptr;

	if (*track_id == 0)
		return nullptr;

	return track_id;
}

static InputStreamPtr
OpenQobuzInput(const char *uri, Mutex &mutex, Cond &cond)
{
	assert(qobuz_client != nullptr);

	const char *track_id = ExtractQobuzTrackId(uri);
	if (track_id == nullptr)
		return nullptr;

	// TODO: validate track_id

	return std::make_unique<QobuzInputStream>(uri, track_id, mutex, cond);
}

static std::unique_ptr<RemoteTagScanner>
ScanQobuzTags(const char *uri, RemoteTagHandler &handler)
{
	assert(qobuz_client != nullptr);

	const char *track_id = ExtractQobuzTrackId(uri);
	if (track_id == nullptr)
		return nullptr;

	return std::make_unique<QobuzTagScanner>(*qobuz_client, track_id,
						 handler);
}

const InputPlugin qobuz_input_plugin = {
	"qobuz",
	InitQobuzInput,
	FinishQobuzInput,
	OpenQobuzInput,
	ScanQobuzTags,
};
