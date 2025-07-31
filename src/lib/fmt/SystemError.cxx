// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SystemError.hxx"
#include "ToBuffer.hxx"

std::system_error
VFmtSystemError(std::error_code code,
		fmt::string_view format_str, fmt::format_args args) noexcept
{
	const auto msg = VFmtBuffer<512>(format_str, args);
	return std::system_error{code, msg};
}

#ifdef _WIN32

std::system_error
VFmtLastError(DWORD code,
	      fmt::string_view format_str, fmt::format_args args) noexcept
{
	const auto msg = VFmtBuffer<512>(format_str, args);
	return MakeLastError(code, msg);
}

#endif // _WIN32
