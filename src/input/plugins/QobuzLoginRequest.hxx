// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef QOBUZ_LOGIN_REQUEST_HXX
#define QOBUZ_LOGIN_REQUEST_HXX

#include "lib/curl/Delegate.hxx"
#include "lib/curl/Request.hxx"

class CurlRequest;
struct QobuzSession;

class QobuzLoginHandler {
public:
	virtual void OnQobuzLoginSuccess(QobuzSession &&session) noexcept = 0;
	virtual void OnQobuzLoginError(std::exception_ptr error) noexcept = 0;
};

class QobuzLoginRequest final : DelegateCurlResponseHandler {
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
	/* virtual methods from DelegateCurlResponseHandler */
	std::unique_ptr<CurlResponseParser> MakeParser(unsigned status,
						       Curl::Headers &&headers) override;
	void FinishParser(std::unique_ptr<CurlResponseParser> p) override;

	/* virtual methods from CurlResponseHandler */
	void OnError(std::exception_ptr e) noexcept override;
};

#endif
