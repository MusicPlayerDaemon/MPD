// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FILTER_OBSERVER_HXX
#define MPD_FILTER_OBSERVER_HXX

#include <memory>

class PreparedFilter;
class Filter;

/**
 * A helper class which observes calls to a #PreparedFilter and allows
 * the caller to access the #Filter instances created by it.
 */
class FilterObserver {
	class PreparedProxy;
	class Proxy;

	PreparedProxy *proxy = nullptr;

public:
	/**
	 * @return a proxy object
	 */
	std::unique_ptr<PreparedFilter> Set(std::unique_ptr<PreparedFilter> pf);

	Filter *Get() noexcept;
};

#endif
