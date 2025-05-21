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

#include <array>

std::system_error
VFmtLastError(DWORD code,
	      fmt::string_view format_str, fmt::format_args args) noexcept
{
	std::array<char, 512> buffer;

	constexpr std::size_t max_prefix = sizeof(buffer) - 128;
	auto [p, _] = fmt::vformat_to_n(buffer.data(),
					buffer.size() - max_prefix,
					format_str, args);
	*p = '\0';

	return MakeLastError(code, buffer.data());
}

#endif // _WIN32
