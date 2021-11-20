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

#include "Init.hxx"
#include "Global.hxx"
#include "event/Call.hxx"
#include "thread/Mutex.hxx"

#include <cassert>

Mutex CurlInit::mutex;
unsigned CurlInit::ref;
CurlGlobal *CurlInit::instance;

CurlInit::CurlInit(EventLoop &event_loop)
{
	const std::scoped_lock<Mutex> protect(mutex);
	if (++ref > 1) {
		assert(&event_loop == &instance->GetEventLoop());
		return;
	}

	CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
	if (code != CURLE_OK)
		throw std::runtime_error(curl_easy_strerror(code));

	assert(instance == nullptr);
	instance = new CurlGlobal(event_loop);
}

CurlInit::~CurlInit() noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);
	if (--ref > 0)
		return;

	BlockingCall(instance->GetEventLoop(), [](){
			delete instance;
			instance = nullptr;
		});

	curl_global_cleanup();
}
