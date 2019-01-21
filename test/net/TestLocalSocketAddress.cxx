/*
 * Copyright 2012-2019 Max Kellermann <max.kellermann@gmail.com>
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

#include "net/AllocatedSocketAddress.hxx"
#include "net/ToString.hxx"

#include <gtest/gtest.h>

#include <sys/un.h>

TEST(LocalSocketAddress, Path)
{
	const char *path = "/run/foo/bar.socket";
	AllocatedSocketAddress a;
	a.SetLocal(path);
	EXPECT_FALSE(a.IsNull());
	EXPECT_TRUE(a.IsDefined());
	EXPECT_EQ(a.GetFamily(), AF_LOCAL);
	EXPECT_EQ(ToString(a), path);

	const auto &sun = *(const struct sockaddr_un *)a.GetAddress();
	EXPECT_STREQ(sun.sun_path, path);
	EXPECT_EQ(sun.sun_path + strlen(path) + 1, (const char *)a.GetAddress() + a.GetSize());
}

#ifdef __linux__

TEST(LocalSocketAddress, Abstract)
{
	const char *path = "@foo.bar";
	AllocatedSocketAddress a;
	a.SetLocal(path);
	EXPECT_FALSE(a.IsNull());
	EXPECT_TRUE(a.IsDefined());
	EXPECT_EQ(a.GetFamily(), AF_LOCAL);
	EXPECT_EQ(ToString(a), path);

	const auto &sun = *(const struct sockaddr_un *)a.GetAddress();

	/* Linux's abstract sockets start with a null byte, ... */
	EXPECT_EQ(sun.sun_path[0], 0);

	/* ... but are not null-terminated */
	EXPECT_EQ(memcmp(sun.sun_path + 1, path + 1, strlen(path) - 1), 0);
	EXPECT_EQ(sun.sun_path + strlen(path), (const char *)a.GetAddress() + a.GetSize());
}

#endif
