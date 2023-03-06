// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INPUT_INIT_HXX
#define MPD_INPUT_INIT_HXX

struct ConfigData;
class EventLoop;

/**
 * Initializes this library and all #InputStream implementations.
 */
void
input_stream_global_init(const ConfigData &config, EventLoop &event_loop);

/**
 * Deinitializes this library and all #InputStream implementations.
 */
void
input_stream_global_finish() noexcept;

class ScopeInputPluginsInit {
public:
	ScopeInputPluginsInit(const ConfigData &config,
			      EventLoop &event_loop) {
		input_stream_global_init(config, event_loop);
	}

	~ScopeInputPluginsInit() noexcept {
		input_stream_global_finish();
	}
};

#endif
