// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "RuntimeError.hxx"
#include "ToBuffer.hxx"

std::runtime_error
VFmtRuntimeError(fmt::string_view format_str, fmt::format_args args) noexcept
{
	const auto msg = VFmtBuffer<512>(format_str, args);
	return std::runtime_error{msg};
}

std::invalid_argument
VFmtInvalidArgument(fmt::string_view format_str, fmt::format_args args) noexcept
{
	const auto msg = VFmtBuffer<512>(format_str, args);
	return std::invalid_argument{msg};
}
