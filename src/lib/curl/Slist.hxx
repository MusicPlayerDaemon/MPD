/*
 * Copyright 2008-2018 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef CURL_SLIST_HXX
#define CURL_SLIST_HXX

#include <curl/curl.h>

#include <stdexcept>
#include <utility>

/**
 * OO wrapper for "struct curl_slist *".
 */
class CurlSlist {
	struct curl_slist *head = nullptr;

public:
	CurlSlist() noexcept = default;

	CurlSlist(CurlSlist &&src) noexcept
		:head(std::exchange(src.head, nullptr)) {}

	~CurlSlist() noexcept {
		if (head != nullptr)
			curl_slist_free_all(head);
	}

	CurlSlist &operator=(CurlSlist &&src) noexcept {
		std::swap(head, src.head);
		return *this;
	}

	struct curl_slist *Get() noexcept {
		return head;
	}

	void Clear() noexcept {
		curl_slist_free_all(head);
		head = nullptr;
	}

	void Append(const char *value) {
		auto *new_head = curl_slist_append(head, value);
		if (new_head == nullptr)
			throw std::runtime_error("curl_slist_append() failed");
		head = new_head;
	}
};

#endif
