// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "net/AllocatedSocketAddress.hxx"
#include "net/LocalSocketAddress.hxx"
#include "net/ToString.hxx"

#include <gtest/gtest.h>

#include <sys/un.h>

using std::string_view_literals::operator""sv;

TEST(LocalSocketAddress, Path1)
{
	const char *path = "/run/foo/bar.socket";
	LocalSocketAddress a;
	a.SetLocal(path);
	EXPECT_TRUE(a.IsDefined());
	EXPECT_EQ(a.GetFamily(), AF_LOCAL);
	EXPECT_EQ(ToString(a), path);
	EXPECT_EQ(a.GetLocalRaw(), "/run/foo/bar.socket\0"sv);
	EXPECT_STREQ(a.GetLocalPath(), path);

	const struct sockaddr *const sa = a;
	const auto &sun = *(const struct sockaddr_un *)sa;
	EXPECT_STREQ(sun.sun_path, path);
	EXPECT_EQ(sun.sun_path + strlen(path) + 1, (const char *)sa + a.GetSize());
}

TEST(LocalSocketAddress, Path2)
{
	static constexpr const char *path = "/run/foo/bar.socket";
	static constexpr LocalSocketAddress a{path};
	EXPECT_TRUE(a.IsDefined());
	EXPECT_EQ(a.GetFamily(), AF_LOCAL);
	EXPECT_EQ(ToString(a), path);
	EXPECT_EQ(a.GetLocalRaw(), "/run/foo/bar.socket\0"sv);
	EXPECT_STREQ(a.GetLocalPath(), path);

	const struct sockaddr *const sa = a;
	const auto &sun = *(const struct sockaddr_un *)sa;
	EXPECT_STREQ(sun.sun_path, path);
	EXPECT_EQ(sun.sun_path + strlen(path) + 1, (const char *)sa + a.GetSize());
}

TEST(LocalSocketAddress, Path)
{
	const char *path = "/run/foo/bar.socket";
	AllocatedSocketAddress a;
	a.SetLocal(path);
	EXPECT_FALSE(a.IsNull());
	EXPECT_TRUE(a.IsDefined());
	EXPECT_EQ(a.GetFamily(), AF_LOCAL);
	EXPECT_EQ(ToString(a), path);
	EXPECT_EQ(a.GetLocalRaw(), "/run/foo/bar.socket\0"sv);
	EXPECT_STREQ(a.GetLocalPath(), path);

	const auto &sun = *(const struct sockaddr_un *)a.GetAddress();
	EXPECT_STREQ(sun.sun_path, path);
	EXPECT_EQ(sun.sun_path + strlen(path) + 1, (const char *)a.GetAddress() + a.GetSize());
}

#ifdef __linux__

TEST(LocalSocketAddress, Abstract1)
{
	const char *path = "@foo.bar";
	LocalSocketAddress a;
	a.SetLocal(path);
	EXPECT_TRUE(a.IsDefined());
	EXPECT_EQ(a.GetFamily(), AF_LOCAL);
	EXPECT_EQ(ToString(a), path);
	EXPECT_EQ(a.GetLocalRaw(), "\0foo.bar"sv);
	EXPECT_EQ(a.GetLocalPath(), nullptr);

	const struct sockaddr *const sa = a;
	const auto &sun = *(const struct sockaddr_un *)sa;

	/* Linux's abstract sockets start with a null byte, ... */
	EXPECT_EQ(sun.sun_path[0], 0);

	/* ... but are not null-terminated */
	EXPECT_EQ(memcmp(sun.sun_path + 1, path + 1, strlen(path) - 1), 0);
	EXPECT_EQ(sun.sun_path + strlen(path), (const char *)sa + a.GetSize());
}

TEST(LocalSocketAddress, Abstract)
{
	const char *path = "@foo.bar";
	AllocatedSocketAddress a;
	a.SetLocal(path);
	EXPECT_FALSE(a.IsNull());
	EXPECT_TRUE(a.IsDefined());
	EXPECT_EQ(a.GetFamily(), AF_LOCAL);
	EXPECT_EQ(ToString(a), path);
	EXPECT_EQ(a.GetLocalRaw(), "\0foo.bar"sv);
	EXPECT_EQ(a.GetLocalPath(), nullptr);

	const auto &sun = *(const struct sockaddr_un *)a.GetAddress();

	/* Linux's abstract sockets start with a null byte, ... */
	EXPECT_EQ(sun.sun_path[0], 0);

	/* ... but are not null-terminated */
	EXPECT_EQ(memcmp(sun.sun_path + 1, path + 1, strlen(path) - 1), 0);
	EXPECT_EQ(sun.sun_path + strlen(path), (const char *)a.GetAddress() + a.GetSize());
}

#endif
