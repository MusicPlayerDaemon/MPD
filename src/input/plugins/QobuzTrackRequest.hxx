// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "lib/curl/Slist.hxx"
#include "lib/curl/Request.hxx"
#include "lib/curl/StringHandler.hxx"

class QobuzClient;
struct QobuzSession;

class QobuzTrackHandler {
public:
	virtual void OnQobuzTrackSuccess(std::string url) noexcept = 0;
	virtual void OnQobuzTrackError(std::exception_ptr error) noexcept = 0;
};

class QobuzTrackRequest final : StringCurlResponseHandler {
	CurlSlist request_headers;

	CurlRequest request;

	QobuzTrackHandler &handler;

public:
	class ResponseParser;

	QobuzTrackRequest(QobuzClient &client, const QobuzSession &session,
			  const char *track_id,
			  QobuzTrackHandler &_handler);

	~QobuzTrackRequest() noexcept;

	void Start() noexcept {
		request.StartIndirect();
	}

private:
	/* virtual methods from CurlResponseHandler */
	void OnEnd() override;
	void OnError(std::exception_ptr e) noexcept override;
};
