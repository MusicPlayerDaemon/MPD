// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "AsyncRequest.hxx"
#include "Setup.hxx"
#include "Global.hxx"
#include "event/Call.hxx"

#include <curl/curl.h>

#include <cassert>

namespace Curl {

AsyncRequest::AsyncRequest(CurlGlobal &_global, CurlEasy &&_easy,
			   CurlResponseHandler &_handler)
	:global(_global), handler(_handler), easy(std::move(_easy))
{
	SetupEasy();
}

AsyncRequest::AsyncRequest(CurlGlobal &_global,
			   CurlResponseHandler &_handler)
	:global(_global), handler(_handler)
{
	Curl::Setup(easy);
	SetupEasy();
}

AsyncRequest::~AsyncRequest() noexcept
{
	FreeEasy();
}

void
AsyncRequest::SetupEasy()
{
	easy.SetPrivate((void *)this);

	handler.Install(easy);
}

void
AsyncRequest::Start()
{
	assert(!registered);

	global.Add(*this);
	registered = true;
}

void
AsyncRequest::StartIndirect()
{
	BlockingCall(global.GetEventLoop(), [this](){
			Start();
		});
}

void
AsyncRequest::Stop() noexcept
{
	if (!registered)
		return;

	global.Remove(*this);
	registered = false;
}

void
AsyncRequest::StopIndirect()
{
	BlockingCall(global.GetEventLoop(), [this](){
			Stop();
		});
}

void
AsyncRequest::FreeEasy() noexcept
{
	if (!easy)
		return;

	Stop();
	easy = nullptr;
}

void
AsyncRequest::Resume() noexcept
{
	assert(registered);

	easy.Unpause();

	global.InvalidateSockets();
}

void
AsyncRequest::Done(CURLcode result) noexcept
{
	Stop();

	handler.Done(result);
}

} // namespace Curl
