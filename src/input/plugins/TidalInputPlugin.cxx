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
#include "TidalInputPlugin.hxx"
#include "TidalSessionManager.hxx"
#include "TidalTrackRequest.hxx"
#include "CurlInputPlugin.hxx"
#include "PluginUnavailable.hxx"
#include "input/ProxyInputStream.hxx"
#include "input/FailingInputStream.hxx"
#include "input/InputPlugin.hxx"
#include "config/Block.hxx"
#include "thread/Mutex.hxx"
#include "util/StringCompare.hxx"

#include <stdexcept>
#include <memory>

static TidalSessionManager *tidal_session;

class TidalInputStream final
	: public ProxyInputStream, TidalSessionHandler, TidalTrackHandler {

	const std::string track_id;

	std::unique_ptr<TidalTrackRequest> track_request;

	std::exception_ptr error;

public:
	TidalInputStream(const char *_uri, const char *_track_id,
			 Mutex &_mutex, Cond &_cond) noexcept
		:ProxyInputStream(_uri, _mutex, _cond),
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
							      mutex, cond));
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
		TidalTrackHandler &handler = *this;
		track_request = std::make_unique<TidalTrackRequest>(tidal_session->GetCurl(),
								    tidal_session->GetBaseUrl(),
								    tidal_session->GetToken(),
								    tidal_session->GetSession().c_str(),
								    track_id.c_str(),
								    handler);
		track_request->Start();
	} catch (...) {
		Failed(std::current_exception());
	}
}

void
TidalInputStream::OnTidalTrackSuccess(std::string url) noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	try {
		SetInput(OpenCurlInputStream(url.c_str(), {},
					     mutex, cond));
	} catch (...) {
		Failed(std::current_exception());
	}
}

void
TidalInputStream::OnTidalTrackError(std::exception_ptr e) noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	Failed(e);
}

static void
InitTidalInput(EventLoop &event_loop, const ConfigBlock &block)
{
	const char *base_url = block.GetBlockValue("base_url",
						   "https://api.tidal.com/v1");

	const char *token = block.GetBlockValue("token");
	if (token == nullptr)
		throw PluginUnavailable("No Tidal application token configured");

	const char *username = block.GetBlockValue("username");
	if (username == nullptr)
		throw PluginUnavailable("No Tidal username configured");

	const char *password = block.GetBlockValue("password");
	if (password == nullptr)
		throw PluginUnavailable("No Tidal password configured");

	// TODO: "audioquality" setting

	tidal_session = new TidalSessionManager(event_loop, base_url, token,
						username, password);
}

static void
FinishTidalInput()
{
	delete tidal_session;
}

static InputStreamPtr
OpenTidalInput(const char *uri, Mutex &mutex, Cond &cond)
{
	assert(tidal_session != nullptr);

	const char *track_id;

	track_id = StringAfterPrefix(uri, "tidal://track/");
	if (track_id == nullptr)
		track_id = StringAfterPrefix(uri, "https://listen.tidal.com/track/");

	if (track_id == nullptr || *track_id == 0)
		return nullptr;

	// TODO: validate track_id

	return std::make_unique<TidalInputStream>(uri, track_id, mutex, cond);
}

const InputPlugin tidal_input_plugin = {
	"tidal",
	InitTidalInput,
	FinishTidalInput,
	OpenTidalInput,
};
