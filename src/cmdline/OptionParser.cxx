// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "OptionParser.hxx"
#include "OptionDef.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/StringCompare.hxx"

static const char *
Shift(std::span<const char *const> &s) noexcept
{
	const char *value = s.front();
	s = s.subspan(1);
	return value;
}

inline const char *
OptionParser::CheckShiftValue(const char *s, const OptionDef &option)
{
	if (!option.HasValue())
		return nullptr;

	if (args.empty())
		throw FmtRuntimeError("Value expected after {}", s);

	return Shift(args);
}

inline OptionParser::Result
OptionParser::IdentifyOption(const char *s)
{
	assert(s != nullptr);
	assert(*s == '-');

	if (s[1] == '-') {
		for (const auto &i : options) {
			if (!i.HasLongOption())
				continue;

			const char *t = StringAfterPrefix(s + 2, i.GetLongOption());
			if (t == nullptr)
				continue;

			const char *value;

			if (*t == 0)
				value = CheckShiftValue(s, i);
			else if (*t == '=')
				value = t + 1;
			else
				continue;

			return {int(&i - options.data()), value};
		}
	} else if (s[1] != 0 && s[2] == 0) {
		const char ch = s[1];
		for (const auto &i : options) {
			if (i.HasShortOption() && ch == i.GetShortOption()) {
				const char *value = CheckShiftValue(s, i);
				return {int(&i - options.data()), value};
			}
		}
	}

	throw FmtRuntimeError("Unknown option: {}", s);
}

OptionParser::Result
OptionParser::Next()
{
	while (!args.empty()) {
		const char *arg = Shift(args);
		if (arg[0] == '-')
			return IdentifyOption(arg);

		*remaining_tail++ = arg;
	}

	return {-1, nullptr};
}
