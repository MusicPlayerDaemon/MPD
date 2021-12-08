/*
 * Copyright 2008-2021 Max Kellermann <max.kellermann@gmail.com>
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

#include "config.h"
#include "Request.hxx"
#include "Global.hxx"
#include "event/Call.hxx"
#include "util/StringStrip.hxx"
#include "Version.h"

#include <curl/curl.h>

#include <cassert>

#include <stdio.h>

CurlRequest::CurlRequest(CurlGlobal &_global, CurlEasy _easy,
			 CurlResponseHandler &_handler)
	:global(_global), handler(_handler), easy(std::move(_easy))
{
	SetupEasy();
}

CurlRequest::CurlRequest(CurlGlobal &_global,
			 CurlResponseHandler &_handler)
	:global(_global), handler(_handler)
{
	SetupEasy();
}

CurlRequest::~CurlRequest() noexcept
{
	FreeEasy();
}

void
CurlRequest::SetupEasy()
{
	easy.SetPrivate((void *)this);

	handler.Install(easy);

	easy.SetUserAgent("Music Player Daemon " VERSION);
#if !defined(ANDROID) && !defined(_WIN32)
	easy.SetOption(CURLOPT_NETRC, 1L);
#endif
	easy.SetNoProgress();
	easy.SetNoSignal();
	easy.SetConnectTimeout(10);
	easy.SetOption(CURLOPT_HTTPAUTH, (long) CURLAUTH_ANY);
}

void
CurlRequest::Start()
{
	assert(!registered);

	global.Add(*this);
	registered = true;
}

void
CurlRequest::StartIndirect()
{
	BlockingCall(global.GetEventLoop(), [this](){
			Start();
		});
}

void
CurlRequest::Stop() noexcept
{
	if (!registered)
		return;

	global.Remove(*this);
	registered = false;
}

void
CurlRequest::StopIndirect()
{
	BlockingCall(global.GetEventLoop(), [this](){
			Stop();
		});
}

void
CurlRequest::FreeEasy() noexcept
{
	if (!easy)
		return;

	Stop();
	easy = nullptr;
}

void
CurlRequest::Resume() noexcept
{
	assert(registered);

	easy.Unpause();

	global.InvalidateSockets();
}

void
CurlRequest::Done(CURLcode result) noexcept
{
	Stop();

	handler.Done(result);
}
