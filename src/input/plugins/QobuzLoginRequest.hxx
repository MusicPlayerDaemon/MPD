// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "lib/curl/Request.hxx"
#include "lib/curl/StringHandler.hxx"

struct QobuzSession;

class QobuzLoginHandler {
public:
	virtual void OnQobuzLoginSuccess(QobuzSession &&session) noexcept = 0;
	virtual void OnQobuzLoginError(std::exception_ptr error) noexcept = 0;
};

class QobuzLoginRequest final : StringCurlResponseHandler {
	CurlRequest request;

	QobuzLoginHandler &handler;

public:
	class ResponseParser;

	QobuzLoginRequest(CurlGlobal &curl,
			  const char *base_url, const char *app_id,
			  const char *username, const char *email,
			  const char *password,
			  const char *device_manufacturer_id,
			  QobuzLoginHandler &_handler);

	~QobuzLoginRequest() noexcept;

	void Start() noexcept {
		request.StartIndirect();
	}

private:
	/* virtual methods from CurlResponseHandler */
	void OnEnd() override;
	void OnError(std::exception_ptr e) noexcept override;
};
