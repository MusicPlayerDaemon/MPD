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
#include "lib/crypto/MD5.hxx"
#include "thread/Mutex.hxx"
#include "util/StringCompare.hxx"

#include <memory>

static QobuzClient *qobuz_client;

class QobuzInputStream final
	: public ProxyInputStream, QobuzSessionHandler, QobuzTrackHandler {

	const std::string track_id;

	std::unique_ptr<QobuzTrackRequest> track_request;

	std::exception_ptr error;

public:
	QobuzInputStream(const char *_uri, const char *_track_id,
			 Mutex &_mutex) noexcept
		:ProxyInputStream(_uri, _mutex),
		 track_id(_track_id)
	{
		qobuz_client->AddLoginHandler(*this);
	}

	~QobuzInputStream() override {
		qobuz_client->RemoveLoginHandler(*this);
	}

	QobuzInputStream(const QobuzInputStream &) = delete;
	QobuzInputStream &operator=(const QobuzInputStream &) = delete;

	/* virtual methods from InputStream */

	void Check() override {
		if (error)
			std::rethrow_exception(error);
	}

private:
	void Failed(const std::exception_ptr& e) {
		SetInput(std::make_unique<FailingInputStream>(GetURI(), e,
							      mutex));
	}

	/* virtual methods from QobuzSessionHandler */
	void OnQobuzSession() noexcept override;

	/* virtual methods from QobuzTrackHandler */
	void OnQobuzTrackSuccess(std::string url) noexcept override;
	void OnQobuzTrackError(std::exception_ptr error) noexcept override;
};

void
QobuzInputStream::OnQobuzSession() noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);

	try {
		const auto session = qobuz_client->GetSession();

		QobuzTrackHandler &h = *this;
		track_request = std::make_unique<QobuzTrackRequest>(*qobuz_client,
								    session,
								    track_id.c_str(),
								    h);
		track_request->Start();
	} catch (...) {
		Failed(std::current_exception());
	}
}

void
QobuzInputStream::OnQobuzTrackSuccess(std::string url) noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);
	track_request.reset();

	try {
		SetInput(OpenCurlInputStream(url.c_str(), {},
					     mutex));
	} catch (...) {
		Failed(std::current_exception());
	}
}

void
QobuzInputStream::OnQobuzTrackError(std::exception_ptr e) noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);
	track_request.reset();

	Failed(e);
}

static void
InitQobuzInput(EventLoop &event_loop, const ConfigBlock &block)
{
	GlobalInitMD5();

	const char *base_url = block.GetBlockValue("base_url",
						   "http://www.qobuz.com/api.json/0.2/");

	const char *app_id = block.GetBlockValue("app_id");
	if (app_id == nullptr)
		throw PluginUnconfigured("No Qobuz app_id configured");

	const char *app_secret = block.GetBlockValue("app_secret");
	if (app_secret == nullptr)
		throw PluginUnconfigured("No Qobuz app_secret configured");

	const char *device_manufacturer_id = block.GetBlockValue("device_manufacturer_id",
								 "df691fdc-fa36-11e7-9718-635337d7df8f");

	const char *username = block.GetBlockValue("username");
	const char *email = block.GetBlockValue("email");
	if (username == nullptr && email == nullptr)
		throw PluginUnconfigured("No Qobuz username configured");

	const char *password = block.GetBlockValue("password");
	if (password == nullptr)
		throw PluginUnconfigured("No Qobuz password configured");

	const char *format_id = block.GetBlockValue("format_id", "5");

	qobuz_client = new QobuzClient(event_loop, base_url,
				       app_id, app_secret,
				       device_manufacturer_id,
				       username, email, password,
				       format_id);
}

static void
FinishQobuzInput() noexcept
{
	delete qobuz_client;
}

[[gnu::pure]]
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
OpenQobuzInput(const char *uri, Mutex &mutex)
{
	assert(qobuz_client != nullptr);

	const char *track_id = ExtractQobuzTrackId(uri);
	if (track_id == nullptr)
		return nullptr;

	// TODO: validate track_id

	return std::make_unique<QobuzInputStream>(uri, track_id, mutex);
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

static constexpr const char *qobuz_prefixes[] = {
	"qobuz://",
	nullptr
};

const InputPlugin qobuz_input_plugin = {
	"qobuz",
	qobuz_prefixes,
	InitQobuzInput,
	FinishQobuzInput,
	OpenQobuzInput,
	nullptr,
	ScanQobuzTags,
};
