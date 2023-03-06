// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Client.hxx"
#include "Domain.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "Log.hxx"

void
Client::OnSocketError(std::exception_ptr ep) noexcept
{
	FmtError(client_domain, "error on client {}: {}", num, ep);

	SetExpired();
}

void
Client::OnSocketClosed() noexcept
{
	SetExpired();
}
