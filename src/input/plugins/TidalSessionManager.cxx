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

#include "TidalSessionManager.hxx"
#include "util/Domain.hxx"

#include "Log.hxx"

static constexpr Domain tidal_domain("tidal");

TidalSessionManager::TidalSessionManager(EventLoop &event_loop,
					 const char *_base_url, const char *_token,
					 const char *_username,
					 const char *_password)
	:base_url(_base_url), token(_token),
	 username(_username), password(_password),
	 curl(event_loop),
	 defer_invoke_handlers(event_loop,
			       BIND_THIS_METHOD(InvokeHandlers))
{
}

TidalSessionManager::~TidalSessionManager() noexcept
{
	assert(handlers.empty());
}

void
TidalSessionManager::AddLoginHandler(TidalSessionHandler &h) noexcept
{
	const std::lock_guard<Mutex> protect(mutex);
	assert(!h.is_linked());

	const bool was_empty = handlers.empty();
	handlers.push_front(h);

	if (!was_empty || login_request)
		return;

	if (session.empty()) {
		// TODO: throttle login attempts?

		LogDebug(tidal_domain, "Sending login request");

		std::string login_uri(base_url);
		login_uri += "/login/username";

		try {
			TidalLoginHandler &handler = *this;
			login_request =
				std::make_unique<TidalLoginRequest>(*curl, base_url,
								    token,
								    username, password,
								    handler);
			login_request->Start();
		} catch (...) {
			error = std::current_exception();
			ScheduleInvokeHandlers();
			return;
		}
	} else
		ScheduleInvokeHandlers();
}

void
TidalSessionManager::OnTidalLoginSuccess(std::string _session) noexcept
{
	FormatDebug(tidal_domain, "Login successful, session=%s", _session.c_str());

	{
		const std::lock_guard<Mutex> protect(mutex);
		login_request.reset();
		session = std::move(_session);
	}

	ScheduleInvokeHandlers();
}

void
TidalSessionManager::OnTidalLoginError(std::exception_ptr e) noexcept
{
	{
		const std::lock_guard<Mutex> protect(mutex);
		login_request.reset();
		error = e;
	}

	ScheduleInvokeHandlers();
}

void
TidalSessionManager::InvokeHandlers() noexcept
{
	const std::lock_guard<Mutex> protect(mutex);
	while (!handlers.empty()) {
		auto &h = handlers.front();
		handlers.pop_front();

		const ScopeUnlock unlock(mutex);
		h.OnTidalSession();
	}
}
