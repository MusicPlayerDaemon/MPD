/*
 * Copyright 2016-2021 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CURL_EASY_HXX
#define CURL_EASY_HXX

#include "String.hxx"

#include <curl/curl.h>

#include <utility>
#include <stdexcept>
#include <cstddef>

/**
 * An OO wrapper for a "CURL*" (a libCURL "easy" handle).
 */
class CurlEasy {
	CURL *handle = nullptr;

public:
	/**
	 * Allocate a new CURL*.
	 *
	 * Throws std::runtime_error on error.
	 */
	CurlEasy()
		:handle(curl_easy_init())
	{
		if (handle == nullptr)
			throw std::runtime_error("curl_easy_init() failed");
	}

	explicit CurlEasy(const char *url)
		:CurlEasy() {
		SetURL(url);
	}

	/**
	 * Create an empty instance.
	 */
	CurlEasy(std::nullptr_t) noexcept:handle(nullptr) {}

	CurlEasy(CurlEasy &&src) noexcept
		:handle(std::exchange(src.handle, nullptr)) {}

	~CurlEasy() noexcept {
		if (handle != nullptr)
			curl_easy_cleanup(handle);
	}

	operator bool() const noexcept {
		return handle != nullptr;
	}

	CurlEasy &operator=(CurlEasy &&src) noexcept {
		std::swap(handle, src.handle);
		return *this;
	}

	CURL *Get() noexcept {
		return handle;
	}

	template<typename T>
	void SetOption(CURLoption option, T value) {
		CURLcode code = curl_easy_setopt(handle, option, value);
		if (code != CURLE_OK)
			throw std::runtime_error(curl_easy_strerror(code));
	}

	void SetPrivate(void *pointer) {
		SetOption(CURLOPT_PRIVATE, pointer);
	}

	void SetErrorBuffer(char *buf) {
		SetOption(CURLOPT_ERRORBUFFER, buf);
	}

	void SetURL(const char *value) {
		SetOption(CURLOPT_URL, value);
	}

	void SetUserAgent(const char *value) {
		SetOption(CURLOPT_USERAGENT, value);
	}

	void SetRequestHeaders(struct curl_slist *headers) {
		SetOption(CURLOPT_HTTPHEADER, headers);
	}

	void SetBasicAuth(const char *userpwd) {
		SetOption(CURLOPT_USERPWD, userpwd);
	}

	void SetUpload(bool value=true) {
		SetOption(CURLOPT_UPLOAD, (long)value);
	}

	void SetNoProgress(bool value=true) {
		SetOption(CURLOPT_NOPROGRESS, (long)value);
	}

	void SetNoSignal(bool value=true) {
		SetOption(CURLOPT_NOSIGNAL, (long)value);
	}

	void SetFailOnError(bool value=true) {
		SetOption(CURLOPT_FAILONERROR, (long)value);
	}

	void SetVerifyHost(bool value) {
		SetOption(CURLOPT_SSL_VERIFYHOST, value ? 2L : 0L);
	}

	void SetVerifyPeer(bool value) {
		SetOption(CURLOPT_SSL_VERIFYPEER, (long)value);
	}

	void SetConnectTimeout(long timeout) {
		SetOption(CURLOPT_CONNECTTIMEOUT, timeout);
	}

	void SetTimeout(long timeout) {
		SetOption(CURLOPT_TIMEOUT, timeout);
	}

	void SetHeaderFunction(size_t (*function)(char *buffer, size_t size,
						  size_t nitems,
						  void *userdata),
			       void *userdata) {
		SetOption(CURLOPT_HEADERFUNCTION, function);
		SetOption(CURLOPT_HEADERDATA, userdata);
	}

	void SetWriteFunction(size_t (*function)(char *ptr, size_t size,
						 size_t nmemb, void *userdata),
			      void *userdata) {
		SetOption(CURLOPT_WRITEFUNCTION, function);
		SetOption(CURLOPT_WRITEDATA, userdata);
	}

	void SetReadFunction(size_t (*function)(char *ptr, size_t size,
						size_t nmemb, void *userdata),
			      void *userdata) {
		SetOption(CURLOPT_READFUNCTION, function);
		SetOption(CURLOPT_READDATA, userdata);
	}

	void SetNoBody(bool value=true) {
		SetOption(CURLOPT_NOBODY, (long)value);
	}

	void SetPost(bool value=true) {
		SetOption(CURLOPT_POST, (long)value);
	}

	void SetRequestBody(const void *data, size_t size) {
		SetOption(CURLOPT_POSTFIELDS, data);
		SetOption(CURLOPT_POSTFIELDSIZE, (long)size);
	}

	void SetHttpPost(const struct curl_httppost *post) {
		SetOption(CURLOPT_HTTPPOST, post);
	}

	template<typename T>
	bool GetInfo(CURLINFO info, T value_r) const noexcept {
		return ::curl_easy_getinfo(handle, info, value_r) == CURLE_OK;
	}

	/**
	 * Returns the response body's size, or -1 if that is unknown.
	 */
	[[gnu::pure]]
	int64_t GetContentLength() const noexcept {
		double value;
		return GetInfo(CURLINFO_CONTENT_LENGTH_DOWNLOAD, &value)
			? (int64_t)value
			: -1;
	}

	void Perform() {
		CURLcode code = curl_easy_perform(handle);
		if (code != CURLE_OK)
			throw std::runtime_error(curl_easy_strerror(code));
	}

	bool Unpause() noexcept {
		return ::curl_easy_pause(handle, CURLPAUSE_CONT) == CURLE_OK;
	}

	CurlString Escape(const char *string, int length=0) const noexcept {
		return CurlString(curl_easy_escape(handle, string, length));
	}
};

#endif
