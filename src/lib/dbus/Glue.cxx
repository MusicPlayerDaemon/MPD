// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Glue.hxx"
#include "event/Call.hxx"

namespace ODBus {

void
Glue::ConnectIndirect()
{
	BlockingCall(GetEventLoop(), [this](){ Connect(); });
}

void
Glue::DisconnectIndirect() noexcept
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
Glue::Disconnect() noexcept
{
	GetConnection().Close();

	watch.SetConnection(Connection());
}

void
Glue::OnDBusClosed() noexcept
{
	// TODO: reconnect
}

} // namespace ODBus
