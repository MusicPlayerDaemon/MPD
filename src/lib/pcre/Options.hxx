// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <pcre2.h>

namespace Pcre {

struct CompileOptions {
	bool anchored = false;
	bool caseless = false;
	bool capture = false;

	explicit constexpr operator int() const noexcept {
		int options = PCRE2_DOTALL|PCRE2_NO_AUTO_CAPTURE;

		if (anchored)
			options |= PCRE2_ANCHORED;

		if (caseless)
			options |= PCRE2_CASELESS;

		if (capture)
			options &= ~PCRE2_NO_AUTO_CAPTURE;

		return options;
	}
};

} // namespace Pcre
