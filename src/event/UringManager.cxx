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

#include "UringManager.hxx"
#include "util/PrintException.hxx"

namespace Uring {

void
Manager::OnSocketReady(unsigned) noexcept
{
	try {
		DispatchCompletions();
	} catch (...) {
		PrintException(std::current_exception());
	}
}

void
Manager::OnIdle() noexcept
{
	try {
		Submit();
	} catch (...) {
		PrintException(std::current_exception());
	}
}

} // namespace Uring
