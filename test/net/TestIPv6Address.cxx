/*
 * Copyright 2012-2020 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "net/IPv6Address.hxx"
#include "net/ToString.hxx"

#include <gtest/gtest.h>

#include <stdexcept>

#ifndef _WIN32
#include <arpa/inet.h>
#endif

static std::string
ToString(const struct in6_addr &a)
{
#ifdef _WIN32
	/* on mingw32, the parameter is non-const (PVOID) */
	const auto p = const_cast<struct in6_addr *>(&a);
#else
	const auto p = &a;
#endif

	char buffer[256];
	const char *result = inet_ntop(AF_INET6, p, buffer, sizeof(buffer));
	if (result == nullptr)
		throw std::runtime_error("inet_ntop() failed");
	return result;
}

TEST(IPv6Address, Octets)
{
	static constexpr auto a = IPv6Address(0x1110, 0x2220, 0x3330, 0x4440,
					      0x5550, 0x6660, 0x7770, 0x8880,
					      1234);
	EXPECT_EQ(ToString(a.GetAddress()), "1110:2220:3330:4440:5550:6660:7770:8880");
}

TEST(IPv6Address, Any)
{
	EXPECT_EQ(ToString(IPv6Address(1234).GetAddress()), "::");
	EXPECT_EQ(ToString(IPv6Address(1234)), "[::]:1234");
}

TEST(IPv6Address, Port)
{
	EXPECT_EQ(IPv6Address(0).GetPort(), 0);
	EXPECT_EQ(IPv6Address(1).GetPort(), 1);
	EXPECT_EQ(IPv6Address(1234).GetPort(), 1234);
	EXPECT_EQ(IPv6Address(0xffff).GetPort(), 0xffff);
}

TEST(IPv6Address, MaskFromPrefix)
{
	EXPECT_EQ(ToString(IPv6Address::MaskFromPrefix(0).GetAddress()), "::");
	EXPECT_EQ(ToString(IPv6Address::MaskFromPrefix(1).GetAddress()), "8000::");
	EXPECT_EQ(ToString(IPv6Address::MaskFromPrefix(8).GetAddress()), "ff00::");
	EXPECT_EQ(ToString(IPv6Address::MaskFromPrefix(16).GetAddress()), "ffff::");
	EXPECT_EQ(ToString(IPv6Address::MaskFromPrefix(17).GetAddress()), "ffff:8000::");
	EXPECT_EQ(ToString(IPv6Address::MaskFromPrefix(32).GetAddress()), "ffff:ffff::");
	EXPECT_EQ(ToString(IPv6Address::MaskFromPrefix(64).GetAddress()), "ffff:ffff:ffff:ffff::");
	EXPECT_TRUE(/* glibc: */
		    ToString(IPv6Address::MaskFromPrefix(112).GetAddress()) == "ffff:ffff:ffff:ffff:ffff:ffff:ffff:0" ||
		    /* macOS: */
		    ToString(IPv6Address::MaskFromPrefix(112).GetAddress()) == "ffff:ffff:ffff:ffff:ffff:ffff:ffff::");
	EXPECT_EQ(ToString(IPv6Address::MaskFromPrefix(128).GetAddress()), "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
}
