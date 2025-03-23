// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "OptionParser.hxx"
#include "OptionDef.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/StringCompare.hxx"

#include <string_view>

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

const char *
OptionParser::PeekOptionValue(std::string_view s)
{
    const OptionDef *target = nullptr;
    for (const auto &def : options) {
        if (def.HasLongOption() && s == def.GetLongOption()) {
            target = &def;
            break;
        }
    }
    if (!target)
        throw FmtRuntimeError("Unknown option definition: {}", s);

    // Work on a copy of the argument list so we don't modify the parser state.
    auto args_copy = args;

    while (!args_copy.empty()) {
        const char *arg = args_copy.front();
        args_copy = args_copy.subspan(1);

        if (arg[0] == '-' && arg[1] == '-') {
            const char *opt = arg + 2;
            std::string_view opt_view(opt);

            // Look for '=' to distinguish between --option and --option=value
            size_t eq_pos = opt_view.find('=');
            std::string_view name = (eq_pos == std::string_view::npos)
                                        ? opt_view
                                        : opt_view.substr(0, eq_pos);
            if (name == s) {
                if (eq_pos != std::string_view::npos) {
                    // Return the value after '='.
                    return opt + eq_pos + 1;
                } else {
                    // Option was provided without an attached value.
                    // If the option expects a value, then the value is the next argument.
                    if (target->HasValue()) {
                        if (args_copy.empty())
                            throw FmtRuntimeError("Value expected after --{}", s);
                        return args_copy.front();
                    }

                    // For flag options, return an empty string (indicating the option is present).
                    return "";
                }
            }
        }
    }

    return nullptr;
}
