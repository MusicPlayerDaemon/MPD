// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Setup.hxx"
#include "Easy.hxx"
#include "Version.h"

namespace Curl {

void
Setup(CurlEasy &easy)
{
	easy.SetUserAgent("Music Player Daemon " VERSION);
#if !defined(ANDROID) && !defined(_WIN32)
	easy.SetOption(CURLOPT_NETRC, 1L);
#endif
	easy.SetNoProgress();
	easy.SetNoSignal();
	easy.SetConnectTimeout(std::chrono::seconds{10});
	easy.SetOption(CURLOPT_HTTPAUTH, (long) CURLAUTH_ANY);
}

} // namespace Curl
