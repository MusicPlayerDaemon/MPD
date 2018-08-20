/*
 * Copyright 2009-2018 Max Kellermann <max.kellermann@gmail.com>
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

#include "StringStrip.hxx"
#include "CharUtil.hxx"

#include <string.h>

const char *
StripLeft(const char *p) noexcept
{
	while (IsWhitespaceNotNull(*p))
		++p;

	return p;
}

const char *
StripLeft(const char *p, const char *end) noexcept
{
	while (p < end && IsWhitespaceOrNull(*p))
		++p;

	return p;
}

const char *
StripRight(const char *p, const char *end) noexcept
{
	while (end > p && IsWhitespaceOrNull(end[-1]))
		--end;

	return end;
}

size_t
StripRight(const char *p, size_t length) noexcept
{
	while (length > 0 && IsWhitespaceOrNull(p[length - 1]))
		--length;

	return length;
}

void
StripRight(char *p) noexcept
{
	size_t old_length = strlen(p);
	size_t new_length = StripRight(p, old_length);
	p[new_length] = 0;
}

char *
Strip(char *p) noexcept
{
	p = StripLeft(p);
	StripRight(p);
	return p;
}
