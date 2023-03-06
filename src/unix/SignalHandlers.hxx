// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SIGNAL_HANDLERS_HXX
#define MPD_SIGNAL_HANDLERS_HXX

struct Instance;

void
SignalHandlersInit(Instance &instance, bool daemon);

void
SignalHandlersFinish() noexcept;

class ScopeSignalHandlersInit {
public:
	ScopeSignalHandlersInit(Instance &instance, bool daemon) {
		SignalHandlersInit(instance, daemon);
	}

	~ScopeSignalHandlersInit() noexcept {
		SignalHandlersFinish();
	}
};

#endif
