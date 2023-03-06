// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef QOBUZ_CLIENT_HXX
#define QOBUZ_CLIENT_HXX

#include "QobuzSession.hxx"
#include "QobuzLoginRequest.hxx"
#include "lib/curl/Init.hxx"
#include "lib/curl/Headers.hxx"
#include "thread/Mutex.hxx"
#include "event/DeferEvent.hxx"
#include "util/IntrusiveList.hxx"

#include <memory>
#include <string>

class QobuzSessionHandler
	: public SafeLinkIntrusiveListHook
{
public:
	virtual void OnQobuzSession() noexcept = 0;
};

class QobuzClient final : QobuzLoginHandler {
	const char *const base_url;
	const char *const app_id, *const app_secret;
	const char *const device_manufacturer_id;
	const char *const username, *const email, *const password;
	const char *const format_id;

	CurlInit curl;

	DeferEvent defer_invoke_handlers;

	/**
	 * Protects #session, #error, #login_request, #handlers.
	 */
	mutable Mutex mutex;

	QobuzSession session;

	std::exception_ptr error;

	using LoginHandlerList = IntrusiveList<QobuzSessionHandler>;

	LoginHandlerList handlers;

	std::unique_ptr<QobuzLoginRequest> login_request;

public:
	QobuzClient(EventLoop &event_loop,
		    const char *_base_url,
		    const char *_app_id, const char *_app_secret,
		    const char *_device_manufacturer_id,
		    const char *_username, const char *_email,
		    const char *_password,
		    const char *_format_id);

	const char *GetFormatId() const noexcept {
		return format_id;
	}

	[[gnu::pure]]
	CurlGlobal &GetCurl() noexcept;

	void AddLoginHandler(QobuzSessionHandler &h) noexcept;

	void RemoveLoginHandler(QobuzSessionHandler &h) noexcept {
		const std::scoped_lock<Mutex> protect(mutex);
		if (h.is_linked())
			h.unlink();
	}

	/**
	 * Throws on error.
	 */
	QobuzSession GetSession() const;

	std::string MakeUrl(const char *object, const char *method,
			    const Curl::Headers &query) const noexcept;

	std::string MakeSignedUrl(const char *object, const char *method,
				  const Curl::Headers &query) const noexcept;

private:
	void StartLogin();

	void InvokeHandlers() noexcept;

	void ScheduleInvokeHandlers() noexcept {
		defer_invoke_handlers.Schedule();
	}

	/* virtual methods from QobuzLoginHandler */
	void OnQobuzLoginSuccess(QobuzSession &&session) noexcept override;
	void OnQobuzLoginError(std::exception_ptr error) noexcept override;
};

#endif
