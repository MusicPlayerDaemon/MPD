// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Easy.hxx"
#include "Adapter.hxx"

class CurlGlobal;
class CurlResponseHandler;

namespace Curl {

/**
 * A non-blocking HTTP request integrated via #CurlGlobal into the
 * #EventLoop.
 *
 * To start sending the request, call Start().
 */
class AsyncRequest final {
	CurlGlobal &global;

	CurlResponseHandlerAdapter handler;

	/** the curl handle */
	CurlEasy easy;

	bool registered = false;

public:
	/**
	 * @param easy a preconfigured #CurlEasy instance;
	 * Curl::Setup() will not be called on it
	 */
	AsyncRequest(CurlGlobal &_global, CurlEasy &&easy,
		    CurlResponseHandler &_handler);

	AsyncRequest(CurlGlobal &_global,
		    CurlResponseHandler &_handler);

	AsyncRequest(CurlGlobal &_global, const char *url,
		    CurlResponseHandler &_handler)
		:AsyncRequest(_global, _handler) {
		easy.SetURL(url);
	}

	~AsyncRequest() noexcept;

	AsyncRequest(const AsyncRequest &) = delete;
	AsyncRequest &operator=(const AsyncRequest &) = delete;

	/**
	 * Register this request via CurlGlobal::Add(), which starts
	 * the request.
	 *
	 * This method must be called in the event loop thread.
	 */
	void Start();

	/**
	 * A thread-safe version of Start().
	 */
	void StartIndirect();

	/**
	 * Unregister this request via CurlGlobal::Remove().
	 *
	 * This method must be called in the event loop thread.
	 */
	void Stop() noexcept;

	/**
	 * A thread-safe version of Stop().
	 */
	void StopIndirect();

	CURL *Get() noexcept {
		return easy.Get();
	}

	/**
	 * Provide access to the underlying #CurlEasy instance, which
	 * allows the caller to configure options prior to submitting
	 * this request.
	 */
	auto &GetEasy() noexcept {
		return easy;
	}

	void Resume() noexcept;

	/**
	 * A HTTP request is finished.  Called by #CurlGlobal.
	 */
	void Done(CURLcode result) noexcept;

private:
	void SetupEasy();

	/**
	 * Frees the current "libcurl easy" handle, and everything
	 * associated with it.
	 */
	void FreeEasy() noexcept;
};

} // namespace Curl
