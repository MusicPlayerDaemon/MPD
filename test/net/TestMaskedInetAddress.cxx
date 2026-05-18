// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "net/MaskedInetAddress.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/IPv4Address.hxx"
#include "net/Literals.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/Features.hxx"

#ifdef HAVE_IPV6
#include "net/IPv6Address.hxx"
#endif

#ifdef HAVE_UN
#include "net/LocalSocketAddress.hxx"
#endif

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

#ifdef HAVE_IPV6
static constexpr IPv6Address any_v6{0, 0, 0, 0, 0, 0, 0, 0, 42};
static constexpr IPv6Address localhost_v6{0, 0, 0, 0, 0, 0, 0, 1, 42};
#endif

static BareInetAddress
MakeBareInet4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	BareInetAddress address;
	if (!address.CopyFrom(IPv4Address{a, b, c, d, 42}))
		throw std::runtime_error{"BareInetAddress::CopyFrom(IPv4Address) failed"};
	return address;
}

static MaskedInetAddress
MakeMaskedInet4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
		       uint_least8_t prefix_length=32)
{
	assert(prefix_length <= 32);

	return {MakeBareInet4Address(a, b, c, d), static_cast<uint_least8_t>(96 + prefix_length)};
}

TEST(MaskedInetAddress, IPv4)
{
	const auto a = MakeMaskedInet4Address(192, 168, 1, 2);
	EXPECT_TRUE(a.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(a.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(a.Matches(SocketAddress{"10.0.0.1"_ipv4}));
#ifdef HAVE_IPV6
	EXPECT_FALSE(a.Matches(SocketAddress{any_v6}));
	EXPECT_FALSE(a.Matches(SocketAddress{localhost_v6}));
#endif
#ifdef HAVE_UN
	EXPECT_FALSE(a.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_FALSE(a.Matches(LocalSocketAddress{"/run/foo"sv}));
#endif

	const auto b = MakeMaskedInet4Address(192, 168, 1, 0, 24);
	EXPECT_TRUE(b.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_TRUE(b.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(b.Matches(SocketAddress{"10.0.0.1"_ipv4}));
#ifdef HAVE_IPV6
	EXPECT_FALSE(b.Matches(SocketAddress{any_v6}));
	EXPECT_FALSE(b.Matches(SocketAddress{localhost_v6}));
#endif
#ifdef HAVE_UN
	EXPECT_FALSE(b.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_FALSE(b.Matches(LocalSocketAddress{"/run/foo"sv}));
#endif

	const auto c = MakeMaskedInet4Address(0, 0, 0, 0);
	EXPECT_TRUE(c.Matches(SocketAddress{"0.0.0.0"_ipv4}));
	EXPECT_FALSE(c.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(c.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(c.Matches(SocketAddress{"10.0.0.1"_ipv4}));
#ifdef HAVE_IPV6
	EXPECT_FALSE(c.Matches(SocketAddress{any_v6}));
	EXPECT_FALSE(c.Matches(SocketAddress{localhost_v6}));
#endif
#ifdef HAVE_UN
	EXPECT_FALSE(c.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_FALSE(c.Matches(LocalSocketAddress{"/run/foo"sv}));
#endif

	const auto d = MakeMaskedInet4Address(0, 0, 0, 0, 0);
	EXPECT_TRUE(d.Matches(SocketAddress{"0.0.0.0"_ipv4}));
	EXPECT_TRUE(d.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_TRUE(d.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_TRUE(d.Matches(SocketAddress{"10.0.0.1"_ipv4}));
#ifdef HAVE_IPV6
	EXPECT_FALSE(d.Matches(SocketAddress{any_v6}));
	EXPECT_FALSE(d.Matches(SocketAddress{localhost_v6}));
#endif
#ifdef HAVE_UN
	EXPECT_FALSE(d.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_FALSE(d.Matches(LocalSocketAddress{"/run/foo"sv}));
#endif
}

TEST(MaskedInetAddress, CopyFromUnspec)
{
	MaskedInetAddress m;

	StaticSocketAddress src{};
	EXPECT_FALSE(m.CopyFrom(src, 0));

	src.SetSize(1);
	EXPECT_FALSE(m.CopyFrom(src, 0));
}

TEST(MaskedInetAddress, CopyFromV4)
{
	MaskedInetAddress m;

	static constexpr IPv4Address zero{0, 0, 0, 0, 42};
	EXPECT_TRUE(m.CopyFrom(zero, 0));
	EXPECT_TRUE(m.CopyFrom(zero, 16));
	EXPECT_TRUE(m.CopyFrom(zero, 32));
	EXPECT_FALSE(m.CopyFrom(zero, 33));

	static constexpr IPv4Address src{192, 168, 1, 0, 42};

	/* host bits not zero */
	EXPECT_FALSE(m.CopyFrom(src, 0));
	EXPECT_FALSE(m.CopyFrom(src, 16));
	EXPECT_FALSE(m.CopyFrom(src, 23));

	/* valid net mask */
	EXPECT_TRUE(m.CopyFrom(src, 24));
	EXPECT_TRUE(m.CopyFrom(src, 25));
	EXPECT_TRUE(m.CopyFrom(src, 32));

	/* prefix too long */
	EXPECT_FALSE(m.CopyFrom(src, 33));
	EXPECT_FALSE(m.CopyFrom(src, 255));
}

TEST(MaskedInetAddress, ParseV4)
{
	const auto a = MakeMaskedInet4Address(192, 168, 1, 2);
	const auto b = MakeMaskedInet4Address(192, 168, 1, 0, 24);
	const auto c = MakeMaskedInet4Address(0, 0, 0, 0, 0);

	MaskedInetAddress x;
	ASSERT_TRUE(x.Parse("192.168.1.2"));
	EXPECT_EQ(x, a);
	ASSERT_TRUE(x.Parse("192.168.1.2/32"));
	EXPECT_EQ(x, a);
	ASSERT_TRUE(x.Parse("192.168.1.0/24"));
	EXPECT_EQ(x, b);
	ASSERT_TRUE(x.Parse("0.0.0.0/0"));
	EXPECT_EQ(x, c);

	ASSERT_FALSE(x.Parse("192.168.1.2/24"));
	ASSERT_FALSE(x.Parse("192.168.1.2/33"));
}

TEST(MaskedInetAddress, FormatV4)
{
	char buffer[256];

	const auto a = MakeMaskedInet4Address(192, 168, 1, 2);
	EXPECT_STREQ(a.Format(buffer), "192.168.1.2");

	const auto b = MakeMaskedInet4Address(192, 168, 1, 0, 24);
	EXPECT_STREQ(b.Format(buffer), "192.168.1.0/24");

	const auto c = MakeMaskedInet4Address(0, 0, 0, 0, 0);
	EXPECT_STREQ(c.Format(buffer), "0.0.0.0/0");
}

#ifdef HAVE_IPV6

TEST(MaskedInetAddress, IPv6)
{
	const MaskedInetAddress a{IPv6Address{0x1234, 0x5678, 0x90ab, 0, 0, 0, 0, 0xcdef, 0}, 128};
	EXPECT_FALSE(a.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(a.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(a.Matches(SocketAddress{any_v6}));
	EXPECT_FALSE(a.Matches(SocketAddress{localhost_v6}));
	EXPECT_TRUE(a.Matches(SocketAddress{IPv6Address{0x1234, 0x5678, 0x90ab, 0, 0, 0, 0, 0xcdef, 42}}));
#ifdef HAVE_UN
	EXPECT_FALSE(a.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_FALSE(a.Matches(LocalSocketAddress{"/run/foo"sv}));
#endif

	const MaskedInetAddress b{IPv6Address{0x1234, 0x5678, 0, 0, 0, 0, 0, 0, 0}, 32};
	EXPECT_FALSE(b.Matches(SocketAddress{"192.168.1.2"_ipv4}));
	EXPECT_FALSE(b.Matches(SocketAddress{"192.168.1.3"_ipv4}));
	EXPECT_FALSE(b.Matches(SocketAddress{any_v6}));
	EXPECT_FALSE(b.Matches(SocketAddress{localhost_v6}));
	EXPECT_TRUE(b.Matches(SocketAddress{IPv6Address{0x1234, 0x5678, 0x90ab, 0, 0, 0, 0, 0xcdef, 42}}));
	EXPECT_TRUE(b.Matches(SocketAddress{IPv6Address{0x1234, 0x5678, 0x90ab, 0, 0, 0, 0, 1, 42}}));
#ifdef HAVE_UN
	EXPECT_FALSE(b.Matches(LocalSocketAddress{"@foo"sv}));
	EXPECT_FALSE(b.Matches(LocalSocketAddress{"/run/foo"sv}));
#endif
}

TEST(MaskedInetAddress, CopyFromV6)
{
	MaskedInetAddress m;

	static constexpr IPv6Address zero{0, 0, 0, 0, 0, 0, 0, 0, 42};
	EXPECT_TRUE(m.CopyFrom(zero, 0));
	EXPECT_TRUE(m.CopyFrom(zero, 16));
	EXPECT_TRUE(m.CopyFrom(zero, 32));
	EXPECT_TRUE(m.CopyFrom(zero, 127));
	EXPECT_TRUE(m.CopyFrom(zero, 128));
	EXPECT_FALSE(m.CopyFrom(zero, 129));

	static constexpr IPv6Address src{0x1234, 0x5678, 0x90ab, 0, 0, 0, 0, 0, 0};

	/* host bits not zero */
	EXPECT_FALSE(m.CopyFrom(src, 0));
	EXPECT_FALSE(m.CopyFrom(src, 16));
	EXPECT_FALSE(m.CopyFrom(src, 47));

	/* valid net mask */
	EXPECT_TRUE(m.CopyFrom(src, 48));
	EXPECT_TRUE(m.CopyFrom(src, 49));
	EXPECT_TRUE(m.CopyFrom(src, 64));
	EXPECT_TRUE(m.CopyFrom(src, 80));
	EXPECT_TRUE(m.CopyFrom(src, 127));
	EXPECT_TRUE(m.CopyFrom(src, 128));

	/* prefix too long */
	EXPECT_FALSE(m.CopyFrom(src, 129));
	EXPECT_FALSE(m.CopyFrom(src, 255));
}

TEST(MaskedInetAddress, ParseV6)
{
	const MaskedInetAddress a{IPv6Address{0x1234, 0x5678, 0x90ab, 0, 0, 0, 0, 0xcdef, 0}, 128};
	const MaskedInetAddress b{IPv6Address{0x1234, 0x5678, 0x90ab, 0, 0, 0, 0, 0, 0}, 48};

	MaskedInetAddress x;
	ASSERT_TRUE(x.Parse("1234:5678:90ab::cdef"));
	EXPECT_EQ(x, a);
	ASSERT_TRUE(x.Parse("1234:5678:90ab::cdef/128"));
	EXPECT_EQ(x, a);
	ASSERT_TRUE(x.Parse("1234:5678:90ab::/48"));
	EXPECT_EQ(x, b);

	ASSERT_FALSE(x.Parse("1234:5678:90ab::cdef/127"));
	ASSERT_FALSE(x.Parse("1234:5678:90ab::cdef/129"));
}

TEST(MaskedInetAddress, FormatV6)
{
	char buffer[256];

	const MaskedInetAddress a{IPv6Address{0x1234, 0x5678, 0x90ab, 0, 0, 0, 0, 0xcdef, 0}, 128};
	EXPECT_STREQ(a.Format(buffer), "1234:5678:90ab::cdef");

	const MaskedInetAddress b{IPv6Address{0x1234, 0x5678, 0x90ab, 0, 0, 0, 0, 0, 0}, 48};
	EXPECT_STREQ(b.Format(buffer), "1234:5678:90ab::/48");
}

#endif // HAVE_IPV6
