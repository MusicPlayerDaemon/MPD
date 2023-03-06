// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef CHROMAPRINT_HXX
#define CHROMAPRINT_HXX

#include "util/ScopeExit.hxx"

#include <chromaprint.h>

#include <span>
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

	void Feed(std::span<const int16_t> src) {
		if (chromaprint_feed(ctx, src.data(), src.size()) != 1)
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
