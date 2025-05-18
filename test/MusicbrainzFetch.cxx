// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ShutdownHandler.hxx"
#include "lib/curl/Global.hxx"
#include "lib/curl/Request.hxx"
#include "lib/curl/StringHandler.hxx"
#include "event/Loop.hxx"
#include "util/PrintException.hxx"
#include "MusicbrainzCache.hxx"
#include <string>
#include <stdio.h>

class ResponseHandler final : public StringCurlResponseHandler {
	EventLoop &event_loop;
	MusicbrainzCache &musicbrainzCache;
	std::exception_ptr error;

public:
	explicit ResponseHandler(EventLoop &_event_loop, MusicbrainzCache &_cache) noexcept
		:event_loop(_event_loop), musicbrainzCache(_cache) {}

	void Finish() {
		if (error)
			std::rethrow_exception(error);
	}

	void OnEnd() override {
		auto resp = StringCurlResponseHandler::GetResponse();

		if (musicbrainzCache.makeTrackInfoFromXml(resp.body))
			musicbrainzCache.printResults();
		else
			throw std::runtime_error("Failed to parse");
		event_loop.Break();
	}

	void OnError(std::exception_ptr e) noexcept override {
		error = std::move(e);
		event_loop.Break();
	}
};


int
main(int argc, char **argv) noexcept
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: FetchMusicbrainz cd-id\n");
		return EXIT_FAILURE;
	}

	std::string cdid(argv[1]);
	std::string urlPrefix("https://musicbrainz.org/ws/2/discid/");
	std::string urlArgs("?inc=artist-credits+recordings+genres");
	std::string url = urlPrefix + cdid + urlArgs;
	EventLoop event_loop;
	const ShutdownHandler shutdown_handler(event_loop);
	CurlGlobal curl_global(event_loop);
	MusicbrainzCache musicbrainzCache;

	ResponseHandler handler(event_loop, musicbrainzCache);
	CurlRequest request(curl_global, url.c_str(), handler);
	request.Start();

	event_loop.Run();

	handler.Finish();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
