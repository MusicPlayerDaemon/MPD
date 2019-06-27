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

#ifndef QOBUZ_CLIENT_HXX
#define QOBUZ_CLIENT_HXX

#include "QobuzSession.hxx"
#include "QobuzLoginRequest.hxx"
#include "lib/curl/Init.hxx"
#include "thread/Mutex.hxx"
#include "event/DeferEvent.hxx"

#include <boost/intrusive/list.hpp>

#include <memory>
#include <map>
#include <string>

class QobuzSessionHandler
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::safe_link>>
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

	typedef boost::intrusive::list<QobuzSessionHandler,
				       boost::intrusive::constant_time_size<false>> LoginHandlerList;

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

	gcc_pure
	CurlGlobal &GetCurl() noexcept;

	void AddLoginHandler(QobuzSessionHandler &h) noexcept;

	void RemoveLoginHandler(QobuzSessionHandler &h) noexcept {
		const std::lock_guard<Mutex> protect(mutex);
		if (h.is_linked())
			handlers.erase(handlers.iterator_to(h));
	}

	/**
	 * Throws on error.
	 */
	QobuzSession GetSession() const;

	std::string MakeUrl(const char *object, const char *method,
			    const std::multimap<std::string, std::string> &query) const noexcept;

	std::string MakeSignedUrl(const char *object, const char *method,
				  const std::multimap<std::string, std::string> &query) const noexcept;

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
