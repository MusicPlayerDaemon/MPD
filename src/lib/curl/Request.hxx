// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef CURL_REQUEST_HXX
#define CURL_REQUEST_HXX

#include "Easy.hxx"
#include "Adapter.hxx"

#include <cstddef>

class CurlGlobal;
class CurlResponseHandler;

/**
 * A non-blocking HTTP request integrated via #CurlGlobal into the
 * #EventLoop.
 *
 * To start sending the request, call Start().
 */
class CurlRequest final {
	CurlGlobal &global;

	CurlResponseHandlerAdapter handler;

	/** the curl handle */
	CurlEasy easy;

	bool registered = false;

public:
	CurlRequest(CurlGlobal &_global, CurlEasy easy,
		    CurlResponseHandler &_handler);

	CurlRequest(CurlGlobal &_global,
		    CurlResponseHandler &_handler);

	CurlRequest(CurlGlobal &_global, const char *url,
		    CurlResponseHandler &_handler)
		:CurlRequest(_global, _handler) {
		SetUrl(url);
	}

	~CurlRequest() noexcept;

	CurlRequest(const CurlRequest &) = delete;
	CurlRequest &operator=(const CurlRequest &) = delete;

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

	template<typename T>
	void SetOption(CURLoption option, T value) {
		easy.SetOption(option, value);
	}

	void SetUrl(const char *url) {
		easy.SetURL(url);
	}

	void SetRequestHeaders(struct curl_slist *request_headers) {
		easy.SetRequestHeaders(request_headers);
	}

	void SetVerifyHost(bool value) {
		easy.SetVerifyHost(value);
	}

	void SetVerifyPeer(bool value) {
		easy.SetVerifyPeer(value);
	}

	void SetProxyVerifyHost(bool value) {
		easy.SetOption(CURLOPT_PROXY_SSL_VERIFYHOST, value ? 2L : 0L);
	}

	void SetProxyVerifyPeer(bool value) {
		easy.SetOption(CURLOPT_PROXY_SSL_VERIFYPEER, value);
	}

	void SetConnectTimeout(long timeout_seconds) {
		easy.SetConnectTimeout(timeout_seconds);
	}

	void SetNoBody(bool value=true) {
		easy.SetNoBody(value);
	}

	void SetPost(bool value=true) {
		easy.SetPost(value);
	}

	void SetRequestBody(const void *data, std::size_t size) {
		easy.SetRequestBody(data, size);
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

#endif
