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

#ifndef TIDAL_SESSION_MANAGER_HXX
#define TIDAL_SESSION_MANAGER_HXX

#include "check.h"
#include "TidalLoginRequest.hxx"
#include "lib/curl/Init.hxx"
#include "thread/Mutex.hxx"
#include "event/DeferEvent.hxx"

#include <boost/intrusive/list.hpp>

#include <memory>
#include <string>

class CurlRequest;
class TidalLoginRequest;

class TidalSessionHandler
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::safe_link>>
{
public:
	virtual void OnTidalSession() noexcept = 0;
};

class TidalSessionManager final : TidalLoginHandler {
	/**
	 * The Tidal API base URL.
	 */
	const char *const base_url;

	/**
	 * The configured Tidal application token.
	 */
	const char *const token;

	/**
	 * The configured Tidal user name.
	 */
	const char *const username;

	/**
	 * The configured Tidal password.
	 */
	const char *const password;

	CurlInit curl;

	DeferEvent defer_invoke_handlers;

	/**
	 * Protects #session, #error and #handlers.
	 */
	mutable Mutex mutex;

	std::exception_ptr error;

	/**
	 * The current Tidal session id, empty if none.
	 */
	std::string session;

	typedef boost::intrusive::list<TidalSessionHandler,
				       boost::intrusive::constant_time_size<false>> LoginHandlerList;

	LoginHandlerList handlers;

	std::unique_ptr<TidalLoginRequest> login_request;

public:
	TidalSessionManager(EventLoop &event_loop,
			    const char *_base_url, const char *_token,
			    const char *_username,
			    const char *_password) noexcept;

	~TidalSessionManager() noexcept;

	EventLoop &GetEventLoop() noexcept {
		return defer_invoke_handlers.GetEventLoop();
	}

	CurlGlobal &GetCurl() noexcept {
		return *curl;
	}

	const char *GetBaseUrl() const noexcept {
		return base_url;
	}

	void AddLoginHandler(TidalSessionHandler &h) noexcept;

	void RemoveLoginHandler(TidalSessionHandler &h) noexcept {
		const std::lock_guard<Mutex> protect(mutex);
		if (h.is_linked())
			handlers.erase(handlers.iterator_to(h));
	}

	const char *GetToken() const noexcept {
		return token;
	}

	std::string GetSession() const {
		const std::lock_guard<Mutex> protect(mutex);

		if (error)
			std::rethrow_exception(error);

		if (session.empty())
			throw std::runtime_error("No session");

		return session;
	}

private:
	void InvokeHandlers() noexcept;

	void ScheduleInvokeHandlers() noexcept {
		defer_invoke_handlers.Schedule();
	}

	/* virtual methods from TidalLoginHandler */
	void OnTidalLoginSuccess(std::string &&session) noexcept override;
	void OnTidalLoginError(std::exception_ptr error) noexcept override;
};

#endif
