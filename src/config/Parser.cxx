// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Parser.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/ByteSizes.hxx"
#include "util/IntOverflow.hxx"
#include "util/NumberParser.hxx"
#include "util/StringStrip.hxx"
#include "util/StringUtil.hxx"

#include <cstdlib>
#include <cerrno>
#include <system_error>

bool
ParseBool(const char *value)
{
	static const char *const t[] = { "yes", "true", "1", nullptr };
	static const char *const f[] = { "no", "false", "0", nullptr };

	if (StringArrayContainsCase(t, value))
		return true;

	if (StringArrayContainsCase(f, value))
		return false;

	throw FmtRuntimeError(R"(Not a valid boolean ("yes" or "no"): {:?})", value);
}

template<std::integral T>
static const char *
ParseIntegerToOrThrow(std::string_view s, T &value)
{
	const auto [ptr, ec] = FromChars(s, value);
	if (ec != std::errc{})
		throw std::system_error{std::make_error_code(ec), "Failed to parse number"};

	return ptr;
}

template<std::integral T>
static T
ParseIntegerOrThrow(std::string_view s)
{
	T value;
	const char *end = ParseIntegerToOrThrow(s, value);
	if (end != s.data() + s.size())
		throw std::invalid_argument{"Garbage after number"};

	return value;
}

unsigned
ParseUnsigned(std::string_view s)
{
	return ParseIntegerOrThrow<unsigned>(s);
}

unsigned
ParsePositive(std::string_view s)
{
	const auto value = ParseUnsigned(s);
	if (value <= 0)
		throw std::runtime_error("Value must be positive");

	return value;
}

double
ParseDouble(const char *s)
{
	char *endptr;
	errno = 0;
	double value = strtod(s, &endptr);
	if (endptr == s || *endptr != 0)
		throw std::runtime_error("Failed to parse number");

	if (errno == ERANGE)
		throw std::system_error{std::make_error_code(std::errc::result_out_of_range),
					"Failed to parse number"};

	return value;
}

template<std::size_t OPERAND>
static std::size_t
Multiply(std::size_t value)
{
	std::size_t result;
	if (MultiplyOverflow(value, OPERAND, result))
		throw std::runtime_error("Value too large");

	return result;
}

std::size_t
ParseSize(const char *s, std::size_t default_factor)
{
	std::size_t value;
	const char *endptr = ParseIntegerToOrThrow(s, value);

	s = StripLeft(endptr);

	bool apply_factor = false;

	switch (*s) {
	case 'k':
	case 'K':
		value = Multiply<KIBI>(value);
		++s;
		break;

	case 'M':
		value = Multiply<MEBI>(value);
		++s;
		break;

	case 'G':
		value = Multiply<GIBI>(value);
		++s;
		break;

	case '\0':
		apply_factor = true;
		break;

	default:
		throw std::runtime_error("Unknown size suffix");
	}

	/* ignore 'B' for "byte" */
	if (*s == 'B') {
		apply_factor = false;
		++s;
	}

	if (*s != '\0')
		throw std::runtime_error("Unknown size suffix");

	if (apply_factor && MultiplyOverflow(value, default_factor, value))
		throw std::runtime_error("Value too large");

	return value;
}

std::chrono::steady_clock::duration
ParseDuration(const char *s)
{
	// TODO: allow unit suffixes
	using Duration = std::chrono::steady_clock::duration;
	using std::chrono::duration_cast;

	const std::chrono::seconds seconds{ParseIntegerOrThrow<std::chrono::seconds::rep>(s)};
	if (seconds > duration_cast<std::chrono::seconds>(Duration::max()) ||
	    seconds < duration_cast<std::chrono::seconds>(Duration::min()))
		throw std::system_error{std::make_error_code(std::errc::result_out_of_range),
					"Failed to parse number"};

	return duration_cast<Duration>(seconds);
}
