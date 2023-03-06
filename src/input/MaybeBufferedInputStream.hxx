// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_MAYBE_BUFFERED_INPUT_STREAM_BUFFER_HXX
#define MPD_MAYBE_BUFFERED_INPUT_STREAM_BUFFER_HXX

#include "ProxyInputStream.hxx"

/**
 * A proxy which automatically inserts #BufferedInputStream once the
 * input becomes ready and is "eligible" (see
 * BufferedInputStream::IsEligible()).
 */
class MaybeBufferedInputStream final : public ProxyInputStream {
public:
	explicit MaybeBufferedInputStream(InputStreamPtr _input) noexcept;

	/* virtual methods from class InputStream */
	void Update() noexcept override;
};

#endif
