// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef QOBUZ_TAG_SCANNER
#define QOBUZ_TAG_SCANNER

#include "lib/curl/Delegate.hxx"
#include "lib/curl/Request.hxx"
#include "input/RemoteTagScanner.hxx"

class QobuzClient;

class QobuzTagScanner final
	: public RemoteTagScanner, DelegateCurlResponseHandler
{
	CurlRequest request;

	RemoteTagHandler &handler;

public:
	class ResponseParser;

	QobuzTagScanner(QobuzClient &client,
			const char *track_id,
			RemoteTagHandler &_handler);

	~QobuzTagScanner() noexcept override;

	void Start() noexcept override {
		request.StartIndirect();
	}

private:
	/* virtual methods from DelegateCurlResponseHandler */
	std::unique_ptr<CurlResponseParser> MakeParser(unsigned status,
						       Curl::Headers &&headers) override;
	void FinishParser(std::unique_ptr<CurlResponseParser> p) override;

	/* virtual methods from CurlResponseHandler */
	void OnError(std::exception_ptr e) noexcept override;
};

#endif
