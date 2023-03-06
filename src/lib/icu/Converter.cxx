// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Converter.hxx"
#include "util/AllocatedString.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "config.h"

#include <fmt/format.h>

#include <iterator>
#include <stdexcept>

#include <string.h>

#ifdef HAVE_ICU
#include "Error.hxx"
#include "Util.hxx"
#include "util/AllocatedArray.hxx"
#include <unicode/ucnv.h>
#elif defined(HAVE_ICONV)
#include "lib/fmt/SystemError.hxx"
#endif

#ifdef HAVE_ICU

IcuConverter::~IcuConverter() noexcept
{
	ucnv_close(converter);
}

#endif

#ifdef HAVE_ICU_CONVERTER

std::unique_ptr<IcuConverter>
IcuConverter::Create(const char *charset)
{
#ifdef HAVE_ICU
	UErrorCode code = U_ZERO_ERROR;
	UConverter *converter = ucnv_open(charset, &code);
	if (converter == nullptr)
		throw ICU::MakeError(code,
				     FmtBuffer<256>(FMT_STRING("Failed to initialize charset '{}'"),
						    charset));

	return std::unique_ptr<IcuConverter>(new IcuConverter(converter));
#elif defined(HAVE_ICONV)
	iconv_t to = iconv_open("utf-8", charset);
	iconv_t from = iconv_open(charset, "utf-8");
	if (to == (iconv_t)-1 || from == (iconv_t)-1) {
		int e = errno;
		if (to != (iconv_t)-1)
			iconv_close(to);
		if (from != (iconv_t)-1)
			iconv_close(from);
		throw FmtErrno(e, FMT_STRING("Failed to initialize charset '{}'"),
			       charset);
	}

	return std::unique_ptr<IcuConverter>(new IcuConverter(to, from));
#endif
}

#ifdef HAVE_ICU
#elif defined(HAVE_ICONV)

static AllocatedString
DoConvert(iconv_t conv, std::string_view src)
{
	// TODO: dynamic buffer?
	char buffer[4096];
	char *in = const_cast<char *>(src.data());
	char *out = buffer;
	size_t in_left = src.size();
	size_t out_left = sizeof(buffer);

	size_t n = iconv(conv, &in, &in_left, &out, &out_left);

	if (n == static_cast<size_t>(-1))
		throw MakeErrno("Charset conversion failed");

	if (in_left > 0)
		throw std::runtime_error("Charset conversion failed");

	return AllocatedString({buffer, sizeof(buffer) - out_left});
}

#endif

AllocatedString
IcuConverter::ToUTF8(std::string_view s) const
{
#ifdef HAVE_ICU
	const std::scoped_lock<Mutex> protect(mutex);

	ucnv_resetToUnicode(converter);

	// TODO: dynamic buffer?
	UChar buffer[4096], *target = buffer;
	const char *source = s.data();

	UErrorCode code = U_ZERO_ERROR;

	ucnv_toUnicode(converter, &target, buffer + std::size(buffer),
		       &source, source + s.size(),
		       nullptr, true, &code);
	if (code != U_ZERO_ERROR)
		throw ICU::MakeError(code, "Failed to convert to Unicode");

	const size_t target_length = target - buffer;
	return UCharToUTF8({buffer, target_length});
#elif defined(HAVE_ICONV)
	return DoConvert(to_utf8, s);
#endif
}

AllocatedString
IcuConverter::FromUTF8(std::string_view s) const
{
#ifdef HAVE_ICU
	const std::scoped_lock<Mutex> protect(mutex);

	const auto u = UCharFromUTF8(s);

	ucnv_resetFromUnicode(converter);

	// TODO: dynamic buffer?
	char buffer[4096], *target = buffer;
	const UChar *source = u.data();
	UErrorCode code = U_ZERO_ERROR;

	ucnv_fromUnicode(converter, &target, buffer + std::size(buffer),
			 &source, u.data() + u.size(),
			 nullptr, true, &code);

	if (code != U_ZERO_ERROR)
		throw ICU::MakeError(code, "Failed to convert from Unicode");

	return {{buffer, size_t(target - buffer)}};

#elif defined(HAVE_ICONV)
	return DoConvert(from_utf8, s);
#endif
}

#endif
