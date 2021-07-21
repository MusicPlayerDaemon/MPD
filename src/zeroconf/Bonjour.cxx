/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Bonjour.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "util/Compiler.h"

#include <dns_sd.h>

#include <stdexcept>

#include <arpa/inet.h>

static constexpr Domain bonjour_domain("bonjour");

/**
 * A wrapper for DNSServiceRegister() which returns the DNSServiceRef
 * and throws on error.
 */
static DNSServiceRef
RegisterBonjour(const char *name, const char *type, unsigned port,
		DNSServiceRegisterReply callback, void *ctx)
{
	DNSServiceRef ref;
	DNSServiceErrorType error = DNSServiceRegister(&ref,
						       0, 0, name, type,
						       nullptr, nullptr,
						       htons(port), 0,
						       nullptr,
						       callback, ctx);

	if (error != kDNSServiceErr_NoError)
		throw std::runtime_error("DNSServiceRegister() failed");

	return ref;
}

BonjourHelper::BonjourHelper(EventLoop &_loop, const char *name,
			     const char *service_type, unsigned port)
	:service_ref(RegisterBonjour(name, service_type, port,
				     Callback, this)),
	 socket_event(_loop,
		      BIND_THIS_METHOD(OnSocketReady),
		      SocketDescriptor(DNSServiceRefSockFD(service_ref)))
{
	socket_event.ScheduleRead();
}

void
BonjourHelper::Callback([[maybe_unused]] DNSServiceRef sdRef,
			[[maybe_unused]] DNSServiceFlags flags,
			DNSServiceErrorType errorCode, const char *name,
			[[maybe_unused]] const char *regtype,
			[[maybe_unused]] const char *domain,
			[[maybe_unused]] void *context) noexcept
{
	auto &helper = *(BonjourHelper *)context;

	if (errorCode != kDNSServiceErr_NoError) {
		LogError(bonjour_domain,
			 "Failed to register zeroconf service");

		helper.Cancel();
	} else {
		FmtDebug(bonjour_domain,
			 "Registered zeroconf service with name '{}'",
			 name);
	}
}

std::unique_ptr<BonjourHelper>
BonjourInit(EventLoop &loop, const char *name,
	    const char *service_type, unsigned port)
{
	return std::make_unique<BonjourHelper>(loop, name, service_type, port);
}
