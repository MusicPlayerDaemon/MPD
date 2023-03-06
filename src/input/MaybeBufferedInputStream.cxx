// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "MaybeBufferedInputStream.hxx"
#include "BufferedInputStream.hxx"

MaybeBufferedInputStream::MaybeBufferedInputStream(InputStreamPtr _input) noexcept
	:ProxyInputStream(std::move(_input)) {}

void
MaybeBufferedInputStream::Update() noexcept
{
	const bool was_ready = IsReady();

	ProxyInputStream::Update();

	if (!was_ready && IsReady() && BufferedInputStream::IsEligible(*input))
		/* our input has just become ready - check if we
		   should buffer it */
		SetInput(std::make_unique<BufferedInputStream>(std::move(input)));
}
