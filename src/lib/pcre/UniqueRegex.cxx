/*
 * Copyright 2007-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
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

#include "UniqueRegex.hxx"
#include "util/RuntimeError.hxx"

void
UniqueRegex::Compile(const char *pattern, bool anchored, bool capture,
		     bool caseless)
{
	constexpr int default_options = PCRE_DOTALL|PCRE_NO_AUTO_CAPTURE|PCRE_UTF8;

	int options = default_options;
	if (anchored)
		options |= PCRE_ANCHORED;
	if (capture)
		options &= ~PCRE_NO_AUTO_CAPTURE;
	if (caseless)
		options |= PCRE_CASELESS;

	const char *error_string;
	int error_offset;
	re = pcre_compile(pattern, options, &error_string, &error_offset, nullptr);
	if (re == nullptr)
		throw FormatRuntimeError("Error in regex at offset %d: %s",
					 error_offset, error_string);

	int study_options = 0;
#ifdef PCRE_CONFIG_JIT
	study_options |= PCRE_STUDY_JIT_COMPILE;
#endif
	extra = pcre_study(re, study_options, &error_string);
	if (extra == nullptr && error_string != nullptr) {
		pcre_free(re);
		re = nullptr;
		throw FormatRuntimeError("Regex study error: %s", error_string);
	}

	int n;
	if (capture && pcre_fullinfo(re, extra, PCRE_INFO_CAPTURECOUNT, &n) == 0)
		n_capture = n;
}
