// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "UniqueRegex.hxx"
#include "Error.hxx"
#include "lib/fmt/ToBuffer.hxx"

void
UniqueRegex::Compile(const char *pattern, const int options)
{
	int error_number;
	PCRE2_SIZE error_offset;
	re = pcre2_compile_8(PCRE2_SPTR8(pattern),
			     PCRE2_ZERO_TERMINATED, options,
			     &error_number, &error_offset,
			     nullptr);
	if (re == nullptr) {
		const auto msg = FmtBuffer<256>("Error in regex at offset {}",
						error_offset);
		throw Pcre::MakeError(error_number, msg);
	}

	pcre2_jit_compile_8(re, PCRE2_JIT_COMPLETE);

	if (int n; (options & PCRE2_NO_AUTO_CAPTURE) == 0 &&
	    pcre2_pattern_info_8(re, PCRE2_INFO_CAPTURECOUNT, &n) == 0)
		n_capture = n;
}
