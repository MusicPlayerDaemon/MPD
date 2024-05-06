// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Helper.hxx"
#include "avahi/Helper.hxx"

ZeroconfHelper::ZeroconfHelper(EventLoop &event_loop, const char *name,
			       const char *service_type, unsigned port)
	:helper(AvahiInit(event_loop, name, service_type, port)) {}

ZeroconfHelper::~ZeroconfHelper() noexcept = default;
