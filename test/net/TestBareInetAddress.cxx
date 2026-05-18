// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "net/BareInetAddress.hxx"
#include "net/IPv4Address.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/Features.hxx"

#ifdef HAVE_IPV6
#include "net/IPv6Address.hxx"
#endif

#ifdef HAVE_UN
#include "net/LocalSocketAddress.hxx"
#endif

#include <gtest/gtest.h>

#include <string_view>

using std::string_view_literals::operator""sv;

TEST(BareInetAddress, CopyFromUnspec)
{
	BareInetAddress address;

	StaticSocketAddress src{};
	EXPECT_FALSE(address.CopyFrom(src));

	src.SetSize(1);
	EXPECT_FALSE(address.CopyFrom(src));

	src.Clear();
	EXPECT_FALSE(address.CopyFrom(src));
}

#ifdef HAVE_UN

TEST(BareInetAddress, CopyFromLocal)
{
	BareInetAddress address;

	EXPECT_FALSE(address.CopyFrom(LocalSocketAddress{}));
	EXPECT_FALSE(address.CopyFrom(LocalSocketAddress{""sv}));
	EXPECT_FALSE(address.CopyFrom(LocalSocketAddress{"/foo"sv}));
	EXPECT_FALSE(address.CopyFrom(LocalSocketAddress{"@foo"sv}));
}

#endif

TEST(BareInetAddress, IsV4Mapped)
{
	BareInetAddress v4;
	ASSERT_TRUE(v4.CopyFrom(IPv4Address{192, 168, 1, 2, 42}));
	EXPECT_TRUE(v4.IsV4Mapped());

#ifdef HAVE_IPV6
	BareInetAddress v6;
	ASSERT_TRUE(v6.CopyFrom(IPv6Address{0x2a00, 0x1450, 0x4001, 0x816, 0, 0, 0, 0x200e, 0}));
	EXPECT_FALSE(v6.IsV4Mapped());
#endif // HAVE_IPV6
}

TEST(BareInetAddress, ParseV4)
{
	BareInetAddress a, b;
	ASSERT_TRUE(a.CopyFrom(IPv4Address{192, 168, 1, 2, 42}));
	ASSERT_TRUE(b.Parse("192.168.1.2"));
	EXPECT_EQ(a, b);
}

#ifdef HAVE_IPV6

TEST(BareInetAddress, ParseV6)
{
	BareInetAddress a, b;
	ASSERT_TRUE(a.CopyFrom(IPv6Address{0x2a00, 0x1450, 0x4001, 0x816, 0, 0, 0, 0x200e, 0}));
	ASSERT_TRUE(b.Parse("2a00:1450:4001:816::200e"));
	EXPECT_EQ(a, b);

	ASSERT_TRUE(a.CopyFrom(IPv6Address{0, 0, 0, 0, 0, 0, 0, 0, 0}));
	ASSERT_TRUE(b.Parse("::"));
	EXPECT_EQ(a, b);
}

#endif // HAVE_IPV6

TEST(BareInetAddress, Format)
{
	char buffer[256];

	BareInetAddress v4;
	ASSERT_TRUE(v4.CopyFrom(IPv4Address{192, 168, 1, 2, 42}));
	EXPECT_STREQ(v4.Format(buffer), "192.168.1.2");

#ifdef HAVE_IPV6
	BareInetAddress v6;
	ASSERT_TRUE(v6.CopyFrom(IPv6Address{0x2a00, 0x1450, 0x4001, 0x816, 0, 0, 0, 0x200e, 0}));
	EXPECT_STREQ(v6.Format(buffer), "2a00:1450:4001:816::200e");

	ASSERT_TRUE(v6.CopyFrom(IPv6Address{0, 0, 0, 0, 0, 0, 0, 0, 0}));
	EXPECT_STREQ(v6.Format(buffer), "::");
#endif // HAVE_IPV6
}

TEST(BareInetAddress, ToNetwork)
{
	char buffer[256];

	BareInetAddress v4;
	ASSERT_TRUE(v4.CopyFrom(IPv4Address{192, 168, 255, 255, 42}));
	EXPECT_STREQ(v4.ToNetwork(96).Format(buffer), "0.0.0.0");
	EXPECT_STREQ(v4.ToNetwork(112).Format(buffer), "192.168.0.0");
	EXPECT_STREQ(v4.ToNetwork(113).Format(buffer), "192.168.128.0");
	EXPECT_STREQ(v4.ToNetwork(120).Format(buffer), "192.168.255.0");
	EXPECT_STREQ(v4.ToNetwork(124).Format(buffer), "192.168.255.240");
	EXPECT_STREQ(v4.ToNetwork(127).Format(buffer), "192.168.255.254");
	EXPECT_STREQ(v4.ToNetwork(128).Format(buffer), "192.168.255.255");

#ifdef HAVE_IPV6
	BareInetAddress v6;
	ASSERT_TRUE(v6.CopyFrom(IPv6Address{0x2a00, 0x1450, 0x4001, 0x816, 0xffff, 0, 0, 0xffff, 0}));
	EXPECT_STREQ(v6.ToNetwork(0).Format(buffer), "::");
	EXPECT_STREQ(v6.ToNetwork(48).Format(buffer), "2a00:1450:4001::");
	EXPECT_STREQ(v6.ToNetwork(64).Format(buffer), "2a00:1450:4001:816::");
	EXPECT_STREQ(v6.ToNetwork(72).Format(buffer), "2a00:1450:4001:816:ff00::");
	EXPECT_STREQ(v6.ToNetwork(120).Format(buffer), "2a00:1450:4001:816:ffff::ff00");
	EXPECT_STREQ(v6.ToNetwork(127).Format(buffer), "2a00:1450:4001:816:ffff::fffe");
	EXPECT_STREQ(v6.ToNetwork(128).Format(buffer), "2a00:1450:4001:816:ffff::ffff");
#endif // HAVE_IPV6
}
