// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "lib/curl/Request.hxx"
#include "lib/curl/StringHandler.hxx"
#include "input/RemoteTagScanner.hxx"

class QobuzClient;

class QobuzTagScanner final
	: public RemoteTagScanner, StringCurlResponseHandler
{
	CurlRequest request;

	RemoteTagHandler &handler;

public:
	class ResponseParser;

	QobuzTagScanner(QobuzClient &client,
			std::string_view track_id,
			RemoteTagHandler &_handler);

	~QobuzTagScanner() noexcept override;

	void Start() noexcept override {
		request.StartIndirect();
	}

private:
	/* virtual methods from CurlResponseHandler */
	void OnEnd() override;
	void OnError(std::exception_ptr e) noexcept override;
};
