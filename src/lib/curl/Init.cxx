// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Init.hxx"
#include "Global.hxx"
#include "Error.hxx"
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
		throw Curl::MakeError(code, "CURL initialization failed");

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
