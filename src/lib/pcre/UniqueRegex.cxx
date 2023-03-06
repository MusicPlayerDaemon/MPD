// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "UniqueRegex.hxx"
#include "Error.hxx"
#include "lib/fmt/ToBuffer.hxx"

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
		const auto msg = FmtBuffer<256>("Error in regex at offset {}",
						error_offset);
		throw Pcre::MakeError(error_number, msg);
	}

	pcre2_jit_compile_8(re, PCRE2_JIT_COMPLETE);

	if (int n; capture &&
	    pcre2_pattern_info_8(re, PCRE2_INFO_CAPTURECOUNT, &n) == 0)
		n_capture = n;
}
