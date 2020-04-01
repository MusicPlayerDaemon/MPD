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

#include "net/IPv4Address.hxx"
#include "net/ToString.hxx"

#include <gtest/gtest.h>

#include <stdexcept>

#ifndef _WIN32
#include <arpa/inet.h>
#endif

static std::string
ToString(const struct in_addr &a)
{
#ifdef _WIN32
	/* on mingw32, the parameter is non-const (PVOID) */
	const auto p = const_cast<struct in_addr *>(&a);
#else
	const auto p = &a;
#endif

	char buffer[256];
	const char *result = inet_ntop(AF_INET, p, buffer, sizeof(buffer));
	if (result == nullptr)
		throw std::runtime_error("inet_ntop() failed");
	return result;
}

TEST(IPv4Address, Octets)
{
	static constexpr auto a = IPv4Address(11, 22, 33, 44, 1234);
	EXPECT_EQ(ToString(a.GetAddress()), "11.22.33.44");
}

TEST(IPv4Address, Any)
{
	EXPECT_EQ(ToString(IPv4Address(1234).GetAddress()), "0.0.0.0");
	EXPECT_EQ(ToString(IPv4Address(1234)), "0.0.0.0:1234");
}

TEST(IPv4Address, Port)
{
	EXPECT_EQ(IPv4Address(0).GetPort(), 0);
	EXPECT_EQ(IPv4Address(1).GetPort(), 1);
	EXPECT_EQ(IPv4Address(1234).GetPort(), 1234);
	EXPECT_EQ(IPv4Address(0xffff).GetPort(), 0xffff);
}

TEST(IPv4Address, Loopback)
{
	static constexpr auto a = IPv4Address(IPv4Address::Loopback(), 1234);
	EXPECT_EQ(ToString(a.GetAddress()), "127.0.0.1");
}

TEST(IPv4Address, MaskFromPrefix)
{
	EXPECT_EQ(ToString(IPv4Address::MaskFromPrefix(0).GetAddress()), "0.0.0.0");
	EXPECT_EQ(ToString(IPv4Address::MaskFromPrefix(4).GetAddress()), "240.0.0.0");
	EXPECT_EQ(ToString(IPv4Address::MaskFromPrefix(8).GetAddress()), "255.0.0.0");
	EXPECT_EQ(ToString(IPv4Address::MaskFromPrefix(16).GetAddress()), "255.255.0.0");
	EXPECT_EQ(ToString(IPv4Address::MaskFromPrefix(24).GetAddress()), "255.255.255.0");
	EXPECT_EQ(ToString(IPv4Address::MaskFromPrefix(31).GetAddress()), "255.255.255.254");
	EXPECT_EQ(ToString(IPv4Address::MaskFromPrefix(32).GetAddress()), "255.255.255.255");
}

TEST(IPv4Address, Numeric)
{
	EXPECT_EQ(IPv4Address(1, 2, 3, 4, 0).GetNumericAddress(), 0x01020304u);
	EXPECT_EQ(IPv4Address(1, 2, 3, 4, 0).GetNumericAddressBE(), htonl(0x01020304));
}
