// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "system/Error.hxx" // IWYU pragma: export

#include <fmt/core.h>
#if FMT_VERSION >= 80000 && FMT_VERSION < 90000
#include <fmt/format.h>
#endif

#include <type_traits>

[[nodiscard]] [[gnu::pure]]
std::system_error
VFmtSystemError(std::error_code code,
		fmt::string_view format_str, fmt::format_args args) noexcept;

template<typename S, typename... Args>
[[nodiscard]] [[gnu::pure]]
std::system_error
FmtSystemError(std::error_code code,
	       const S &format_str, Args&&... args) noexcept
{
#if FMT_VERSION >= 90000
	return VFmtSystemError(code, format_str,
			       fmt::make_format_args(args...));
#else
	return VFmtSystemError(code, fmt::to_string_view(format_str),
			       fmt::make_args_checked<Args...>(format_str,
							       args...));
#endif
}

#ifdef _WIN32

[[nodiscard]] [[gnu::pure]]
std::system_error
VFmtLastError(DWORD code,
	      fmt::string_view format_str, fmt::format_args args) noexcept;

template<typename S, typename... Args>
[[nodiscard]] [[gnu::pure]]
std::system_error
FmtLastError(DWORD code,
	     const S &format_str, Args&&... args) noexcept
{
#if FMT_VERSION >= 90000
	return VFmtLastError(code, format_str,
			     fmt::make_format_args(args...));
#else
	return VFmtLastError(code, fmt::to_string_view(format_str),
			     fmt::make_args_checked<Args...>(format_str,
							     args...));
#endif
}

template<typename S, typename... Args>
[[nodiscard]] [[gnu::pure]]
std::system_error
FmtLastError(const S &format_str, Args&&... args) noexcept
{
	return FmtLastError(GetLastError(),
			    format_str, std::forward<Args>(args)...);
}

#endif // _WIN32

[[nodiscard]] [[gnu::pure]]
inline std::system_error
VFmtErrno(int code,
	  fmt::string_view format_str, fmt::format_args args) noexcept
{
	return VFmtSystemError(std::error_code(code, ErrnoCategory()),
			       format_str, args);
}

template<typename S, typename... Args>
requires(std::is_convertible_v<S, fmt::string_view>)
[[nodiscard]] [[gnu::pure]]
std::system_error
FmtErrno(int code, const S &format_str, Args&&... args) noexcept
{
	return FmtSystemError(std::error_code(code, ErrnoCategory()),
			      format_str, std::forward<Args>(args)...);
}

template<typename S, typename... Args>
requires(std::is_convertible_v<S, fmt::string_view>)
[[nodiscard]] [[gnu::pure]]
std::system_error
FmtErrno(const S &format_str, Args&&... args) noexcept
{
	return FmtErrno(errno, format_str, std::forward<Args>(args)...);
}

template<typename S, typename... Args>
requires(std::is_convertible_v<S, fmt::string_view>)
[[nodiscard]] [[gnu::pure]]
std::system_error
FmtFileNotFound(const S &format_str, Args&&... args) noexcept
{
#ifdef _WIN32
	return FmtLastError(DWORD{ERROR_FILE_NOT_FOUND},
			    format_str, std::forward<Args>(args)...);
#else
	return FmtErrno(ENOENT, format_str, std::forward<Args>(args)...);
#endif
}
