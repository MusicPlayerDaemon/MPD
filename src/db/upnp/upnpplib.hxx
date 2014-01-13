/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef _LIBUPNP_H_X_INCLUDED_
#define _LIBUPNP_H_X_INCLUDED_

#include "util/Error.hxx"

#include <upnp/upnp.h>

#include <functional>

/** Our link to libupnp. Initialize and keep the handle around */
class LibUPnP {
	typedef std::function<void(Upnp_EventType type, void *event)> Handler;

	Error init_error;
	UpnpClient_Handle m_clh;

	Handler handler;

	LibUPnP();

	LibUPnP(const LibUPnP &) = delete;
	LibUPnP &operator=(const LibUPnP &) = delete;

	static int o_callback(Upnp_EventType, void *, void *);

public:
	~LibUPnP();

	/** Retrieve the singleton LibUPnP object */
	static LibUPnP *getLibUPnP(Error &error);

	/** Check state after initialization */
	bool ok() const
	{
		return !init_error.IsDefined();
	}

	/** Retrieve init error if state not ok */
	const Error &GetInitError() const {
		return init_error;
	}

	template<typename T>
	void SetHandler(T &&_handler) {
		handler = std::forward<T>(_handler);
	}

	UpnpClient_Handle getclh()
	{
		return m_clh;
	}
};

#endif /* _LIBUPNP.H_X_INCLUDED_ */
