// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Error.hxx"
#include "String.hxx"

#include <curl/curl.h>

#include <chrono>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

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
	CURLcode TrySetOption(CURLoption option, T value) noexcept {
		return curl_easy_setopt(handle, option, value);
	}

	template<typename T>
	void SetOption(CURLoption option, T value) {
		CURLcode code = TrySetOption(option, value);
		if (code != CURLE_OK)
			throw Curl::MakeError(code, "Failed to set option");
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

	void SetXferInfoFunction(curl_xferinfo_callback function,
				 void *data) {
		SetOption(CURLOPT_XFERINFOFUNCTION, function);
		SetOption(CURLOPT_XFERINFODATA, data);
		SetNoProgress(false);
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

	void SetProxyVerifyHost(bool value) {
		SetOption(CURLOPT_PROXY_SSL_VERIFYHOST, value ? 2L : 0L);
	}

	void SetProxyVerifyPeer(bool value) {
		SetOption(CURLOPT_PROXY_SSL_VERIFYPEER, value);
	}

	void SetConnectTimeout(std::chrono::duration<long> timeout) {
		SetOption(CURLOPT_CONNECTTIMEOUT, timeout.count());
	}

	void SetTimeout(std::chrono::duration<long> timeout) {
		SetOption(CURLOPT_TIMEOUT, timeout.count());
	}

	void SetMaxFileSize(curl_off_t size) {
		SetOption(CURLOPT_MAXFILESIZE_LARGE, size);
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

	void SetRequestBody(std::span<const std::byte> s) {
		SetRequestBody(s.data(), s.size());
	}

	void SetRequestBody(std::string_view s) {
		SetRequestBody(s.data(), s.size());
	}

	void SetMimePost(const curl_mime *mime) {
		SetOption(CURLOPT_MIMEPOST, mime);
	}

	template<typename T>
	bool GetInfo(CURLINFO info, T value_r) const noexcept {
		return ::curl_easy_getinfo(handle, info, value_r) == CURLE_OK;
	}

	/**
	 * Returns the response body's size, or -1 if that is unknown.
	 */
	[[gnu::pure]]
	curl_off_t GetContentLength() const noexcept {
		curl_off_t value;
		return GetInfo(CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &value)
			? value
			: -1;
	}

	void Perform() {
		CURLcode code = curl_easy_perform(handle);
		if (code != CURLE_OK)
			throw Curl::MakeError(code, "CURL failed");
	}

	bool Unpause() noexcept {
		return ::curl_easy_pause(handle, CURLPAUSE_CONT) == CURLE_OK;
	}

	CurlString Escape(const char *string, int length=0) const noexcept {
		return CurlString(curl_easy_escape(handle, string, length));
	}
};
