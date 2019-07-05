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

#include "TidalInputPlugin.hxx"
#include "TidalSessionManager.hxx"
#include "TidalTrackRequest.hxx"
#include "TidalTagScanner.hxx"
#include "TidalError.hxx"
#include "CurlInputPlugin.hxx"
#include "PluginUnavailable.hxx"
#include "input/ProxyInputStream.hxx"
#include "input/FailingInputStream.hxx"
#include "input/InputPlugin.hxx"
#include "config/Block.hxx"
#include "thread/Mutex.hxx"
#include "util/Domain.hxx"
#include "util/Exception.hxx"
#include "util/StringCompare.hxx"
#include "Log.hxx"

#include <memory>

static constexpr Domain tidal_domain("tidal");

static TidalSessionManager *tidal_session;
static const char *tidal_audioquality;

class TidalInputStream final
	: public ProxyInputStream, TidalSessionHandler, TidalTrackHandler {

	const std::string track_id;

	std::unique_ptr<TidalTrackRequest> track_request;

	std::exception_ptr error;

	/**
	 * Retry to login if TidalError::IsInvalidSession() returns
	 * true?
	 */
	bool retry_login = true;

public:
	TidalInputStream(const char *_uri, const char *_track_id,
			 Mutex &_mutex) noexcept
		:ProxyInputStream(_uri, _mutex),
		 track_id(_track_id)
	{
		tidal_session->AddLoginHandler(*this);
	}

	~TidalInputStream() {
		tidal_session->RemoveLoginHandler(*this);
	}

	/* virtual methods from InputStream */

	void Check() override {
		if (error)
			std::rethrow_exception(error);
	}

private:
	void Failed(std::exception_ptr e) {
		SetInput(std::make_unique<FailingInputStream>(GetURI(), e,
							      mutex));
	}

	/* virtual methods from TidalSessionHandler */
	void OnTidalSession() noexcept override;

	/* virtual methods from TidalTrackHandler */
	void OnTidalTrackSuccess(std::string url) noexcept override;
	void OnTidalTrackError(std::exception_ptr error) noexcept override;
};

void
TidalInputStream::OnTidalSession() noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	try {
		TidalTrackHandler &h = *this;
		track_request = std::make_unique<TidalTrackRequest>(tidal_session->GetCurl(),
								    tidal_session->GetBaseUrl(),
								    tidal_session->GetToken(),
								    tidal_session->GetSession().c_str(),
								    track_id.c_str(),
								    tidal_audioquality,
								    h);
		track_request->Start();
	} catch (...) {
		Failed(std::current_exception());
	}
}

void
TidalInputStream::OnTidalTrackSuccess(std::string url) noexcept
{
	FormatDebug(tidal_domain, "Tidal track '%s' resolves to %s",
		    track_id.c_str(), url.c_str());

	const std::lock_guard<Mutex> protect(mutex);

	track_request.reset();

	try {
		SetInput(OpenCurlInputStream(url.c_str(), {},
					     mutex));
	} catch (...) {
		Failed(std::current_exception());
	}
}

gcc_pure
static bool
IsInvalidSession(std::exception_ptr e) noexcept
{
	try {
		std::rethrow_exception(e);
	} catch (const TidalError &te) {
		return te.IsInvalidSession();
	} catch (...) {
		return false;
	}
}

void
TidalInputStream::OnTidalTrackError(std::exception_ptr e) noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	if (retry_login && IsInvalidSession(e)) {
		/* the session has expired - obtain a new session id
		   by logging in again */

		FormatInfo(tidal_domain, "Session expired ('%s'), retrying to log in",
			   GetFullMessage(e).c_str());

		retry_login = false;
		tidal_session->AddLoginHandler(*this);
		return;
	}

	Failed(e);
}

static void
InitTidalInput(EventLoop &event_loop, const ConfigBlock &block)
{
	const char *base_url = block.GetBlockValue("base_url",
						   "https://api.tidal.com/v1");

	const char *token = block.GetBlockValue("token");
	if (token == nullptr)
		throw PluginUnconfigured("No Tidal application token configured");

	const char *username = block.GetBlockValue("username");
	if (username == nullptr)
		throw PluginUnconfigured("No Tidal username configured");

	const char *password = block.GetBlockValue("password");
	if (password == nullptr)
		throw PluginUnconfigured("No Tidal password configured");

	FormatWarning(tidal_domain, "The Tidal input plugin is deprecated because Tidal has changed the protocol and doesn't share documentation");

	tidal_audioquality = block.GetBlockValue("audioquality", "HIGH");

	tidal_session = new TidalSessionManager(event_loop, base_url, token,
						username, password);
}

static void
FinishTidalInput()
{
	delete tidal_session;
}

gcc_pure
static const char *
ExtractTidalTrackId(const char *uri)
{
	const char *track_id = StringAfterPrefix(uri, "tidal://track/");
	if (track_id == nullptr) {
		track_id = StringAfterPrefix(uri, "https://listen.tidal.com/track/");
		if (track_id == nullptr)
			return nullptr;
	}

	if (*track_id == 0)
		return nullptr;

	return track_id;
}

static InputStreamPtr
OpenTidalInput(const char *uri, Mutex &mutex)
{
	assert(tidal_session != nullptr);

	const char *track_id = ExtractTidalTrackId(uri);
	if (track_id == nullptr)
		return nullptr;

	// TODO: validate track_id

	return std::make_unique<TidalInputStream>(uri, track_id, mutex);
}

static std::unique_ptr<RemoteTagScanner>
ScanTidalTags(const char *uri, RemoteTagHandler &handler)
{
	assert(tidal_session != nullptr);

	const char *track_id = ExtractTidalTrackId(uri);
	if (track_id == nullptr)
		return nullptr;

	return std::make_unique<TidalTagScanner>(tidal_session->GetCurl(),
						 tidal_session->GetBaseUrl(),
						 tidal_session->GetToken(),
						 track_id, handler);
}

static constexpr const char *tidal_prefixes[] = {
	"tidal://",
	nullptr
};

const InputPlugin tidal_input_plugin = {
	"tidal",
	tidal_prefixes,
	InitTidalInput,
	FinishTidalInput,
	OpenTidalInput,
	nullptr,
	ScanTidalTags,
};
