/*
 * Copyright (C) 2009-2014 Max Kellermann <max@duempel.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Tokenizer.hxx"
#include "CharUtil.hxx"
#include "StringUtil.hxx"
#include "Error.hxx"
#include "Domain.hxx"

static constexpr Domain tokenizer_domain("tokenizer");

static inline bool
valid_word_first_char(char ch)
{
	return IsAlphaASCII(ch);
}

static inline bool
valid_word_char(char ch)
{
	return IsAlphaNumericASCII(ch) || ch == '_';
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
		if (IsWhitespaceFast(*input)) {
			/* a whitespace: the word ends here */
			*input = 0;
			/* skip all following spaces, too */
			input = StripLeft(input + 1);
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
		if (IsWhitespaceFast(*input)) {
			/* a whitespace: the word ends here */
			*input = 0;
			/* skip all following spaces, too */
			input = StripLeft(input + 1);
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
	if (!IsWhitespaceFast(*input)) {
		error.Set(tokenizer_domain,
			  "Space expected after closing '\"'");
		return nullptr;
	}

	/* finish the string and return it */

	*dest = 0;
	input = StripLeft(input);
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
