/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#ifndef MPD_CHROMAPRINT_HXX
#define MPD_CHROMAPRINT_HXX

#include "util/ScopeExit.hxx"

#include <chromaprint.h>

#include <stdexcept>
#include <string>

namespace Chromaprint {

class Context {
	ChromaprintContext *const ctx;

public:
	Context() noexcept
		:ctx(chromaprint_new(CHROMAPRINT_ALGORITHM_DEFAULT)) {}

	~Context() noexcept {
		chromaprint_free(ctx);
	}

	Context(const Context &) = delete;
	Context &operator=(const Context &) = delete;

	void Start(unsigned sample_rate, unsigned num_channels) {
		if (chromaprint_start(ctx, sample_rate, num_channels) != 1)
			throw std::runtime_error("chromaprint_start() failed");
	}

	void Feed(const int16_t *data, size_t size) {
		if (chromaprint_feed(ctx, data, size) != 1)
			throw std::runtime_error("chromaprint_feed() failed");
	}

	void Finish() {
		if (chromaprint_finish(ctx) != 1)
			throw std::runtime_error("chromaprint_finish() failed");
	}

	std::string GetFingerprint() const {
		char *fingerprint;
		if (chromaprint_get_fingerprint(ctx, &fingerprint) != 1)
			throw std::runtime_error("chromaprint_get_fingerprint() failed");

		AtScopeExit(fingerprint) { chromaprint_dealloc(fingerprint); };
		return fingerprint;
	}
};

} //namespace Chromaprint

#endif
