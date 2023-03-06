// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PREPARED_FILTER_HXX
#define MPD_PREPARED_FILTER_HXX

#include <memory>

struct AudioFormat;
class Filter;

class PreparedFilter {
public:
	virtual ~PreparedFilter() = default;

	/**
	 * Opens the filter, preparing it for FilterPCM().
	 *
	 * Throws on error.
	 *
	 * @param af the audio format of incoming data; the
	 * plugin may modify the object to enforce another input
	 * format
	 */
	virtual std::unique_ptr<Filter> Open(AudioFormat &af) = 0;
};

#endif
