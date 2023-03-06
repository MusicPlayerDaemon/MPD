// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ShutdownHandler.hxx"
#include "lib/curl/Global.hxx"
#include "lib/curl/Request.hxx"
#include "lib/curl/Handler.hxx"
#include "event/Loop.hxx"
#include "util/PrintException.hxx"

#include <stdio.h>

class MyHandler final : public CurlResponseHandler {
	EventLoop &event_loop;

	std::exception_ptr error;

public:
	explicit MyHandler(EventLoop &_event_loop) noexcept
		:event_loop(_event_loop) {}

	void Finish() {
		if (error)
			std::rethrow_exception(error);
	}

	/* virtual methods from CurlResponseHandler */
	void OnHeaders(unsigned status, Curl::Headers &&headers) override {
		fprintf(stderr, "status: %u\n", status);
		for (const auto &i : headers)
			fprintf(stderr, "%s: %s\n",
				i.first.c_str(), i.second.c_str());
	}

	void OnData(std::span<const std::byte> data) override {
		try {
			if (fwrite(data.data(), data.size(), 1, stdout) != 1)
				throw std::runtime_error("Failed to write");
		} catch (...) {
			OnError(std::current_exception());
		}
	}

	void OnEnd() override {
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
		fprintf(stderr, "Usage: RunCurl URI\n");
		return EXIT_FAILURE;
	}

	const char *const uri = argv[1];

	EventLoop event_loop;
	const ShutdownHandler shutdown_handler(event_loop);
	CurlGlobal curl_global(event_loop);

	MyHandler handler(event_loop);
	CurlRequest request(curl_global, uri, handler);
	request.Start();

	event_loop.Run();

	handler.Finish();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
