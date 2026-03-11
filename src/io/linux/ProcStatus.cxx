// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "ProcStatus.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/NumberParser.hxx"
#include "util/StringSplit.hxx"

using std::string_view_literals::operator""sv;

unsigned
ProcStatusThreads() noexcept
{
	UniqueFileDescriptor fd;
	if (!fd.OpenReadOnly("/proc/self/status"))
		return 0;

	char buffer[4096];
	ssize_t nbytes = fd.Read(std::as_writable_bytes(std::span{buffer}));
	if (nbytes <= 0)
		return 0;

	const std::string_view contents{buffer, static_cast<std::size_t>(nbytes)};

	static constexpr std::string_view prefix = "\nThreads:\t"sv;
	const auto position = contents.find(prefix);
	if (position == contents.npos)
		return 0;

	const std::string_view value_string = Split(contents.substr(position + prefix.size()), '\n').first;
	unsigned value;
	if (!ParseIntegerTo(value_string, value))
		return 0;

	return value;
}
