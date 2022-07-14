/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#include "PeakBuffer.hxx"
#include "DynamicFifoBuffer.hxx"

#include <algorithm>
#include <cassert>

PeakBuffer::~PeakBuffer() noexcept
{
	delete normal_buffer;
	delete peak_buffer;
}

bool
PeakBuffer::empty() const noexcept
{
	return (normal_buffer == nullptr || normal_buffer->empty()) &&
		(peak_buffer == nullptr || peak_buffer->empty());
}

std::span<std::byte>
PeakBuffer::Read() const noexcept
{
	if (normal_buffer != nullptr) {
		const auto p = normal_buffer->Read();
		if (!p.empty())
			return p;
	}

	if (peak_buffer != nullptr) {
		const auto p = peak_buffer->Read();
		if (!p.empty())
			return p;
	}

	return {};
}

void
PeakBuffer::Consume(std::size_t length) noexcept
{
	if (normal_buffer != nullptr && !normal_buffer->empty()) {
		normal_buffer->Consume(length);
		return;
	}

	if (peak_buffer != nullptr && !peak_buffer->empty()) {
		peak_buffer->Consume(length);
		if (peak_buffer->empty()) {
			delete peak_buffer;
			peak_buffer = nullptr;
		}

		return;
	}
}

static std::size_t
AppendTo(DynamicFifoBuffer<std::byte> &buffer,
	 std::span<const std::byte> src) noexcept
{
	assert(!src.empty());

	std::size_t total = 0;

	do {
		const auto p = buffer.Write();
		if (p.empty())
			break;

		const std::size_t nbytes = std::min(src.size(), p.size());
		std::copy_n(src.begin(), nbytes, p.begin());
		buffer.Append(nbytes);

		src = src.subspan(nbytes);
		total += nbytes;
	} while (!src.empty());

	return total;
}

bool
PeakBuffer::Append(std::span<const std::byte> src)
{
	if (src.empty())
		return true;

	if (peak_buffer != nullptr && !peak_buffer->empty()) {
		std::size_t nbytes = AppendTo(*peak_buffer, src);
		return nbytes == src.size();
	}

	if (normal_buffer == nullptr)
		normal_buffer = new DynamicFifoBuffer<std::byte>(normal_size);

	std::size_t nbytes = AppendTo(*normal_buffer, src);
	if (nbytes > 0) {
		src = src.subspan(nbytes);
		if (src.empty())
			return true;
	}

	if (peak_buffer == nullptr) {
		if (peak_size > 0)
			peak_buffer = new DynamicFifoBuffer<std::byte>(peak_size);
		if (peak_buffer == nullptr)
			return false;
	}

	nbytes = AppendTo(*peak_buffer, src);
	return nbytes == src.size();
}
