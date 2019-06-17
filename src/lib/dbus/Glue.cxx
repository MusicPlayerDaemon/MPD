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

#include "Glue.hxx"
#include "event/Call.hxx"

namespace ODBus {

void
Glue::ConnectIndirect()
{
	BlockingCall(GetEventLoop(), [this](){ Connect(); });
}

void
Glue::DisconnectIndirect()
{
	BlockingCall(GetEventLoop(), [this](){ Disconnect(); });
}

void
Glue::Connect()
{
	watch.SetConnection(Connection::GetSystemPrivate());

	dbus_connection_set_exit_on_disconnect(GetConnection(), false);
}

void
Glue::Disconnect()
{
	GetConnection().Close();

	watch.SetConnection(Connection());
}

void
Glue::OnDBusClosed() noexcept
{
	// TODO: reconnect
}

}
