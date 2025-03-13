// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "net/Init.hxx"
#include "net/IPv4Address.hxx"
#include "net/ToString.hxx"
#include "util/Compiler.h"

#include <gtest/gtest.h>

#include <stdexcept>

#ifndef _WIN32
#include <arpa/inet.h>
#endif

#if GCC_CHECK_VERSION(11,0)
/* suppress warning for calling GetSize() on uninitialized object */
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

TEST(IPv4AddressTest, Basic)
{
	IPv4Address dummy;
	EXPECT_EQ(dummy.GetSize(), sizeof(struct sockaddr_in));
}

TEST(IPv4AddressTest, Port)
{
	IPv4Address a(12345);
	EXPECT_EQ(a.GetPort(), 12345u);

	a.SetPort(42);
	EXPECT_EQ(a.GetPort(), 42u);
}

TEST(IPv4AddressTest, NumericAddress)
{
	IPv4Address a(12345);
	EXPECT_EQ(a.GetNumericAddress(), 0u);
	EXPECT_EQ(a.GetNumericAddressBE(), 0u);

	a = IPv4Address(192, 168, 1, 2, 42);
	EXPECT_EQ(a.GetNumericAddress(), 0xc0a80102);
	EXPECT_EQ(a.GetNumericAddressBE(), ToBE32(0xc0a80102));
}

TEST(IPv4AddressTest, Mask)
{
	EXPECT_EQ(IPv4Address::MaskFromPrefix(0).GetNumericAddress(),
		  IPv4Address(0, 0, 0, 0, 0).GetNumericAddress());
	EXPECT_EQ(IPv4Address::MaskFromPrefix(1).GetNumericAddress(),
		  IPv4Address(128, 0, 0, 0, 0).GetNumericAddress());
	EXPECT_EQ(IPv4Address::MaskFromPrefix(23).GetNumericAddress(),
		  IPv4Address(255, 255, 254, 0, 0).GetNumericAddress());
	EXPECT_EQ(IPv4Address::MaskFromPrefix(24).GetNumericAddress(),
		  IPv4Address(255, 255, 255, 0, 0).GetNumericAddress());
	EXPECT_EQ(IPv4Address::MaskFromPrefix(32).GetNumericAddress(),
		  IPv4Address(255, 255, 255, 255, 0).GetNumericAddress());
}

TEST(IPv4AddressTest, And)
{
	EXPECT_EQ((IPv4Address::MaskFromPrefix(32) &
		   IPv4Address(192, 168, 1, 2, 0)).GetNumericAddress(),
		  IPv4Address(192, 168, 1, 2, 0).GetNumericAddress());
	EXPECT_EQ((IPv4Address::MaskFromPrefix(24) &
		   IPv4Address(192, 168, 1, 2, 0)).GetNumericAddress(),
		  IPv4Address(192, 168, 1, 0, 0).GetNumericAddress());
	EXPECT_EQ((IPv4Address::MaskFromPrefix(16) &
		   IPv4Address(192, 168, 1, 2, 0)).GetNumericAddress(),
		  IPv4Address(192, 168, 0, 0, 0).GetNumericAddress());
	EXPECT_EQ((IPv4Address::MaskFromPrefix(8) &
		   IPv4Address(192, 168, 1, 2, 0)).GetNumericAddress(),
		  IPv4Address(192, 0, 0, 0, 0).GetNumericAddress());
	EXPECT_EQ((IPv4Address::MaskFromPrefix(0) &
		   IPv4Address(192, 168, 1, 2, 0)).GetNumericAddress(),
		  IPv4Address(0, 0, 0, 0, 0).GetNumericAddress());
}

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
	const ScopeNetInit net_init;

	static constexpr auto a = IPv4Address(11, 22, 33, 44, 1234);
	EXPECT_EQ(ToString(a.GetAddress()), "11.22.33.44");
}

TEST(IPv4Address, Any)
{
	const ScopeNetInit net_init;

	EXPECT_EQ(ToString(IPv4Address(1234).GetAddress()), "0.0.0.0");
	EXPECT_EQ(ToString(IPv4Address(1234)), "0.0.0.0:1234");
}

TEST(IPv4Address, Port)
{
	const ScopeNetInit net_init;

	EXPECT_EQ(IPv4Address(0).GetPort(), 0);
	EXPECT_EQ(IPv4Address(1).GetPort(), 1);
	EXPECT_EQ(IPv4Address(1234).GetPort(), 1234);
	EXPECT_EQ(IPv4Address(0xffff).GetPort(), 0xffff);
}

TEST(IPv4Address, Loopback)
{
	const ScopeNetInit net_init;

	static constexpr auto a = IPv4Address(IPv4Address::Loopback(), 1234);
	EXPECT_EQ(ToString(a.GetAddress()), "127.0.0.1");
}

TEST(IPv4Address, MaskFromPrefix)
{
	const ScopeNetInit net_init;

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
