/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
 * http://www.musicpd.org
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
#include "Tokenizer.hxx"
#include "StringUtil.hxx"
#include "Error.hxx"
#include "Domain.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

static constexpr Domain tokenizer_domain("tokenizer");

static inline bool
valid_word_first_char(char ch)
{
	return g_ascii_isalpha(ch);
}

static inline bool
valid_word_char(char ch)
{
	return g_ascii_isalnum(ch) || ch == '_';
}

char *
Tokenizer::NextWord(Error &error)
{
	char *const word = input;

	if (*input == 0)
		return nullptr;

	/* check the first character */

	if (!valid_word_first_char(*input)) {
		error.Set(tokenizer_domain, "Letter expected");
		return nullptr;
	}

	/* now iterate over the other characters until we find a
	   whitespace or end-of-string */

	while (*++input != 0) {
		if (g_ascii_isspace(*input)) {
			/* a whitespace: the word ends here */
			*input = 0;
			/* skip all following spaces, too */
			input = strchug_fast(input + 1);
			break;
		}

		if (!valid_word_char(*input)) {
			error.Set(tokenizer_domain, "Invalid word character");
			return nullptr;
		}
	}

	/* end of string: the string is already null-terminated
	   here */

	return word;
}

static inline bool
valid_unquoted_char(char ch)
{
	return (unsigned char)ch > 0x20 && ch != '"' && ch != '\'';
}

char *
Tokenizer::NextUnquoted(Error &error)
{
	char *const word = input;

	if (*input == 0)
		return nullptr;

	/* check the first character */

	if (!valid_unquoted_char(*input)) {
		error.Set(tokenizer_domain, "Invalid unquoted character");
		return nullptr;
	}

	/* now iterate over the other characters until we find a
	   whitespace or end-of-string */

	while (*++input != 0) {
		if (g_ascii_isspace(*input)) {
			/* a whitespace: the word ends here */
			*input = 0;
			/* skip all following spaces, too */
			input = strchug_fast(input + 1);
			break;
		}

		if (!valid_unquoted_char(*input)) {
			error.Set(tokenizer_domain,
				  "Invalid unquoted character");
			return nullptr;
		}
	}

	/* end of string: the string is already null-terminated
	   here */

	return word;
}

char *
Tokenizer::NextString(Error &error)
{
	char *const word = input, *dest = input;

	if (*input == 0)
		/* end of line */
		return nullptr;

	/* check for the opening " */

	if (*input != '"') {
		error.Set(tokenizer_domain, "'\"' expected");
		return nullptr;
	}

	++input;

	/* copy all characters */

	while (*input != '"') {
		if (*input == '\\')
			/* the backslash escapes the following
			   character */
			++input;

		if (*input == 0) {
			/* return input-1 so the caller can see the
			   difference between "end of line" and
			   "error" */
			--input;
			error.Set(tokenizer_domain, "Missing closing '\"'");
			return nullptr;
		}

		/* copy one character */
		*dest++ = *input++;
	}

	/* the following character must be a whitespace (or end of
	   line) */

	++input;
	if (*input != 0 && !g_ascii_isspace(*input)) {
		error.Set(tokenizer_domain,
			  "Space expected after closing '\"'");
		return nullptr;
	}

	/* finish the string and return it */

	*dest = 0;
	input = strchug_fast(input);
	return word;
}

char *
Tokenizer::NextParam(Error &error)
{
	if (*input == '"')
		return NextString(error);
	else
		return NextUnquoted(error);
}
