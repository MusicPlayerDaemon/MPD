/*
 * Copyright 2015-2018 Cary Audio
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "Converter.hxx"
#include "Log.hxx"
#include "util/Domain.hxx"

#include <string.h>

namespace Iconv {

static constexpr Domain domain("iconv");

Converter::Converter()
	: handle(nullptr), code(UTF8)
{

}

Converter::~Converter()
{
	if (handle != nullptr) {
		iconv_close(handle);
		handle = nullptr;
	}
}

static const char *code_str[] = {
	"UTF-8",
	"GB2312",
	"BIG5",
	"GBK",
};

static constexpr unsigned char one_byte = 0x0;
static constexpr unsigned char two_byte = 0xc0;
static constexpr unsigned char three_byte = 0xe0;
static constexpr unsigned char four_byte = 0xf0;
static constexpr unsigned char five_byte = 0xf8;
static constexpr unsigned char six_byte = 0xfc;
static constexpr unsigned char one_byte_mask = 0x80;
static constexpr unsigned char two_byte_mask = 0xe0;
static constexpr unsigned char three_byte_mask = 0xf0;
static constexpr unsigned char four_byte_mask = 0xf8;
static constexpr unsigned char five_byte_mask = 0xfc;
static constexpr unsigned char six_byte_mask = 0xfe;

//judge the byte whether begin with binary 10
static inline bool
is_utf8_special_byte(unsigned char c)
{
	return (c & 0xc0) == 0x80;
}

static inline bool
is_ascii(unsigned char c)
{
	return !(c & one_byte_mask);
}

static bool
is_utf8_code(const unsigned char *str, unsigned size)
{
	unsigned char c;

	for (unsigned i=0; i<size;) {
		c = str[i++];
		if (is_ascii(c)) {
			continue;
		} else if ((c & two_byte_mask) == two_byte) {
			if (!is_utf8_special_byte(str[i++])) {
				return false;
			}
		} else if ((c & three_byte_mask) == three_byte) {
			if (!(is_utf8_special_byte(str[i++])
				&& is_utf8_special_byte(str[i++]))) {
				return false;
			}
		} else if ((c & four_byte_mask) == four_byte) {
			if (!(is_utf8_special_byte(str[i++])
				&& is_utf8_special_byte(str[i++])
				&& is_utf8_special_byte(str[i++]))) {
				return false;
			}
		} else if ((c & five_byte_mask) == five_byte) {
			if (!(is_utf8_special_byte(str[i++])
				&& is_utf8_special_byte(str[i++])
				&& is_utf8_special_byte(str[i++])
				&& is_utf8_special_byte(str[i++]))) {
				return false;
			}
		} else if ((c & six_byte_mask) == six_byte) {
			if (!(is_utf8_special_byte(str[i++])
				&& is_utf8_special_byte(str[i++])
				&& is_utf8_special_byte(str[i++])
				&& is_utf8_special_byte(str[i++])
				&& is_utf8_special_byte(str[i++]))) {
				return false;
			}
		} else {
			return false;
		}
	}

	return true;
}

static bool
is_gb2312_code(const unsigned char *str, unsigned size)
{
	unsigned char c;

	for (unsigned i=0; i<size;) {
		c = str[i++];
		if (is_ascii(c)) {
			continue;
		} else if (c >= 0xA1 && c <= 0xF7) {
			c = str[i++];
			if (!(c >= 0XA1 && c <= 0XFE)) {
				return false;
			}
		} else {
			return false;
		}
	}

	return true;
}

static bool
is_big5_code(const unsigned char *str, unsigned size)
{
	unsigned char c;

	for (unsigned i=0; i<size;) {
		c = str[i++];
		if (is_ascii(c)) {
			continue;
		} else if (c >= 0xA1 && c <= 0xF9) {
			c = str[i++];
			if (!((c >= 0x40 && c <= 0x7E)
				|| (c >= 0XA1 && c <= 0xFE))) {
				return false;
			}
		} else {
			return false;
		}
	}

	return true;
}

static bool
is_gbk_code(const unsigned char *str, unsigned size)
{
	unsigned char c;

	for (unsigned i=0; i<size;) {
		c = str[i++];
		if (is_ascii(c)) {
			continue;
		} else if (c >= 0x81 && c <= 0xFE) {
			c = str[i++];
			if (!(c >= 0x40 && c <= 0xFE)) {
				return false;;
			}
		} else {
			return false;
		}
	}

	return true;
}

std::string
Converter::toUTF8(const char *src, size_t inlen)
{
	if (src == nullptr ||
		inlen == 0) {
		return std::string();
	}

	Code c;
	const unsigned char *buf = (const unsigned char *)src;

	if (is_utf8_code(buf, inlen)) {
		return std::string(src, inlen);
	} else if (is_gb2312_code(buf, inlen)) {
		c = GB2312;
	} else if (is_big5_code(buf, inlen)) {
		c = BIG5;
	} else if (is_gbk_code(buf, inlen)) {
		c = GBK;
	} else {
		FormatError(domain, "unkown code: %s", src);
		return std::string(src, inlen);
	}
	if (handle != nullptr && c != code) {
		iconv_close(handle);
		handle = nullptr;
	}
	code = c;
	if (handle == nullptr) {
		handle = iconv_open(code_str[UTF8], code_str[code]);
		if (handle == nullptr) {
			FormatError(domain, "fail iconv_open, code:%s", code_str[code]);
			return std::string(src, inlen);
		}
	}

	char dst[4096];
	memset(dst, 0, sizeof(dst));
	size_t dstlen = sizeof(dst);
	char *in = const_cast<char*>(src);
	char *out = dst;
	size_t n = iconv(handle, &in, &inlen, &out, &dstlen);
	if (n == static_cast<size_t>(-1)) {
		FormatError(domain, "fail iconv: %ld, code:%s", n, code_str[code]);
		return std::string(src, inlen);
	}
	//FormatDefault(domain, "%s -> %s", code_str[code], code_str[UTF8]);

	return std::string(dst, sizeof(dst) - dstlen);
}

}
