// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef CURL_INIT_HXX
#define CURL_INIT_HXX

#include "thread/Mutex.hxx"

class EventLoop;
class CurlGlobal;

/**
 * This class performs one-time initialization of libCURL and creates
 * one #CurlGlobal instance, shared across all #CurlInit instances.
 */
class CurlInit {
	static Mutex mutex;
	static unsigned ref;
	static CurlGlobal *instance;

public:
	explicit CurlInit(EventLoop &event_loop);
	~CurlInit() noexcept;

	CurlInit(const CurlInit &) = delete;
	CurlInit &operator=(const CurlInit &) = delete;

	CurlGlobal &operator*() noexcept {
		return *instance;
	}

	const CurlGlobal &operator*() const noexcept {
		return *instance;
	}

	CurlGlobal *operator->() noexcept {
		return instance;
	}

	const CurlGlobal *operator->() const noexcept {
		return instance;
	}
};

#endif
