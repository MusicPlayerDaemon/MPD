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
