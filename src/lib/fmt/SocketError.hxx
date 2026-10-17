// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "SystemError.hxx"
#include "net/SocketError.hxx"

[[nodiscard]] [[gnu::pure]]
inline std::system_error
VFmtSocketError(socket_error_t code,
		fmt::string_view format_str, fmt::format_args args) noexcept
{
#ifdef _WIN32
	return VFmtLastError(code, format_str, args);
#else
	return VFmtErrno(code, format_str, args);
#endif
}

template<typename S, typename... Args>
[[nodiscard]] [[gnu::pure]]
std::system_error
FmtSocketError(socket_error_t code,
	       const S &format_str, Args&&... args) noexcept
{
	return VFmtSocketError(code, format_str,
			       fmt::make_format_args(args...));
}

template<typename S, typename... Args>
[[nodiscard]] [[gnu::pure]]
std::system_error
FmtSocketError(const S &format_str, Args&&... args) noexcept
{
	return FmtSocketError(GetSocketError(), format_str, std::forward<Args>(args)...);
}
