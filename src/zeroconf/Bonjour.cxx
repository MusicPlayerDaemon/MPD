// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Bonjour.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

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
