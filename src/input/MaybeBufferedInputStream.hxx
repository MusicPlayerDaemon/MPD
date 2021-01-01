/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
