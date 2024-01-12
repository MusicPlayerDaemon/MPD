// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "SocketAddress.hxx"
#include "StaticSocketAddress.hxx"

#include <span>

#include <sys/socket.h>

inline constexpr struct msghdr
MakeMsgHdr(std::span<const struct iovec> iov) noexcept
{
	struct msghdr mh{};
	mh.msg_iov = const_cast<struct iovec *>(iov.data());
	mh.msg_iovlen = iov.size();
	return mh;
}

/**
 * Construct a struct msghdr.  The parameters are `const` because that
 * is needed for sending; but for receiving, these buffers must
 * actually be writable.
 */
inline constexpr struct msghdr
MakeMsgHdr(SocketAddress name, std::span<const struct iovec> iov,
	   std::span<const std::byte> control) noexcept
{
	auto mh = MakeMsgHdr(iov);
	mh.msg_name = const_cast<struct sockaddr *>(name.GetAddress());
	mh.msg_namelen = name.GetSize();
	mh.msg_control = const_cast<std::byte *>(control.data());
	mh.msg_controllen = control.size();
	return mh;
}

inline constexpr struct msghdr
MakeMsgHdr(StaticSocketAddress &name, std::span<const struct iovec> iov,
	   std::span<const std::byte> control) noexcept
{
	auto mh = MakeMsgHdr(iov);
	mh.msg_name = name;
	mh.msg_namelen = name.GetCapacity();
	mh.msg_control = const_cast<std::byte *>(control.data());
	mh.msg_controllen = control.size();
	return mh;
}

inline constexpr struct msghdr
MakeMsgHdr(struct sockaddr_storage &name, std::span<const struct iovec> iov,
	   std::span<const std::byte> control) noexcept
{
	auto mh = MakeMsgHdr(iov);
	mh.msg_name = static_cast<struct sockaddr *>(static_cast<void *>(&name));
	mh.msg_namelen = sizeof(name);
	mh.msg_control = const_cast<std::byte *>(control.data());
	mh.msg_controllen = control.size();
	return mh;
}
