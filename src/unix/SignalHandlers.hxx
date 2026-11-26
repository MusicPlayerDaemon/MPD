// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SIGNAL_HANDLERS_HXX
#define MPD_SIGNAL_HANDLERS_HXX

struct Instance;

void
SignalHandlersInit(Instance &instance);

void
SignalHandlersFinish() noexcept;

class ScopeSignalHandlersInit {
public:
	explicit ScopeSignalHandlersInit(Instance &instance) {
		SignalHandlersInit(instance);
	}

	~ScopeSignalHandlersInit() noexcept {
		SignalHandlersFinish();
	}
};

#endif
