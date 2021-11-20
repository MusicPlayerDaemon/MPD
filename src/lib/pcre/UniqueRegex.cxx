/*
 * Copyright 2007-2021 CM4all GmbH
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
#include "Error.hxx"

#include <stdio.h>

void
UniqueRegex::Compile(const char *pattern, bool anchored, bool capture,
		     bool caseless)
{
	constexpr int default_options = PCRE2_DOTALL|PCRE2_NO_AUTO_CAPTURE;

	uint32_t options = default_options;
	if (anchored)
		options |= PCRE2_ANCHORED;
	if (capture)
		options &= ~PCRE2_NO_AUTO_CAPTURE;
	if (caseless)
		options |= PCRE2_CASELESS;

	int error_number;
	PCRE2_SIZE error_offset;
	re = pcre2_compile_8(PCRE2_SPTR8(pattern),
			     PCRE2_ZERO_TERMINATED, options,
			     &error_number, &error_offset,
			     nullptr);
	if (re == nullptr) {
		char msg[256];
		snprintf(msg, sizeof(msg), "Error in regex at offset %zu",
			 error_offset);
		throw Pcre::MakeError(error_number, msg);
	}

	pcre2_jit_compile_8(re, PCRE2_JIT_COMPLETE);

	if (int n; capture &&
	    pcre2_pattern_info_8(re, PCRE2_INFO_CAPTURECOUNT, &n) == 0)
		n_capture = n;
}
