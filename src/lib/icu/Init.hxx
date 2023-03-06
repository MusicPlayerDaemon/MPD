// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ICU_INIT_HXX
#define MPD_ICU_INIT_HXX

#include "config.h"

#ifdef HAVE_ICU

void
IcuInit();

void
IcuFinish() noexcept;

#else

static inline void IcuInit() noexcept {}
static inline void IcuFinish() noexcept {}

#endif

class ScopeIcuInit {
public:
	ScopeIcuInit() {
		IcuInit();
	}

	~ScopeIcuInit() noexcept {
		IcuFinish();
	}

	ScopeIcuInit(const ScopeIcuInit &) = delete;
	ScopeIcuInit &operator=(const ScopeIcuInit &) = delete;
};

#endif
