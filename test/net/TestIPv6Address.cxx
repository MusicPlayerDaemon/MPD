// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "net/IPv6Address.hxx"
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

TEST(IPv6AddressTest, Basic)
{
	IPv6Address dummy;
	EXPECT_EQ(dummy.GetSize(), sizeof(struct sockaddr_in6));
}

TEST(IPv6AddressTest, Port)
{
	IPv6Address a(12345);
	EXPECT_EQ(a.GetPort(), 12345u);

	a.SetPort(42);
	EXPECT_EQ(a.GetPort(), 42u);
}

static bool
operator==(const struct in6_addr &a, const struct in6_addr &b)
{
	return memcmp(&a, &b, sizeof(a)) == 0;
}

TEST(IPv6AddressTest, Mask)
{
	EXPECT_EQ(IPv6Address::MaskFromPrefix(0).GetAddress(),
		  IPv6Address(0, 0, 0, 0, 0, 0, 0, 0, 0).GetAddress());
	EXPECT_EQ(IPv6Address::MaskFromPrefix(128).GetAddress(),
		  IPv6Address(0xffff, 0xffff, 0xffff, 0xffff,
			      0xffff, 0xffff, 0xffff, 0xffff, 0).GetAddress());
	EXPECT_EQ(IPv6Address::MaskFromPrefix(127).GetAddress(),
		  IPv6Address(0xffff, 0xffff, 0xffff, 0xffff,
			      0xffff, 0xffff, 0xffff, 0xfffe, 0).GetAddress());
	EXPECT_EQ(IPv6Address::MaskFromPrefix(64).GetAddress(),
		  IPv6Address(0xffff, 0xffff, 0xffff, 0xffff,
			      0, 0, 0, 0, 0).GetAddress());
	EXPECT_EQ(IPv6Address::MaskFromPrefix(56).GetAddress(),
		  IPv6Address(0xffff, 0xffff, 0xffff, 0xff00,
			      0, 0, 0, 0, 0).GetAddress());
}

TEST(IPv6AddressTest, And)
{
	EXPECT_EQ((IPv6Address::MaskFromPrefix(128) &
		   IPv6Address::MaskFromPrefix(56)).GetAddress(),
		  IPv6Address::MaskFromPrefix(56).GetAddress());
	EXPECT_EQ((IPv6Address::MaskFromPrefix(48) &
		   IPv6Address(0x2a00, 0x1450, 0x4001, 0x816,
			       0, 0, 0, 0x200e, 0)).GetAddress(),
		  IPv6Address(0x2a00, 0x1450, 0x4001, 0,
			      0, 0, 0, 0, 0).GetAddress());
	EXPECT_EQ((IPv6Address::MaskFromPrefix(24) &
		   IPv6Address(0x2a00, 0x1450, 0x4001, 0x816,
			       0, 0, 0, 0x200e, 0)).GetAddress(),
		  IPv6Address(0x2a00, 0x1400, 0, 0,
			      0, 0, 0, 0, 0).GetAddress());
}

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
