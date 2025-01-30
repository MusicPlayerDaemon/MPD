// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Helper.hxx"

#ifdef HAVE_AVAHI
#include "avahi/Helper.hxx"
#define CreateHelper AvahiInit
#endif

#ifdef HAVE_BONJOUR
#include "Bonjour.hxx"
#define CreateHelper BonjourInit
#endif

ZeroconfHelper::ZeroconfHelper(EventLoop &event_loop, const char *name,
			       const char *service_type, unsigned port)
	:helper(CreateHelper(event_loop, name, service_type, port)) {}

ZeroconfHelper::~ZeroconfHelper() noexcept = default;
