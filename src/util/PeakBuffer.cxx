/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "HugeAllocator.hxx"
#include "fifo_buffer.h"

#include <algorithm>

#include <assert.h>
#include <stdint.h>
#include <string.h>

PeakBuffer::~PeakBuffer()
{
	if (normal_buffer != nullptr)
		fifo_buffer_free(normal_buffer);

	if (peak_buffer != nullptr)
		HugeFree(peak_buffer, peak_size);
}

bool
PeakBuffer::IsEmpty() const
{
	return (normal_buffer == nullptr ||
		fifo_buffer_is_empty(normal_buffer)) &&
		(peak_buffer == nullptr ||
		 fifo_buffer_is_empty(peak_buffer));
}

const void *
PeakBuffer::Read(size_t *length_r) const
{
	if (normal_buffer != nullptr) {
		const void *p = fifo_buffer_read(normal_buffer, length_r);
		if (p != nullptr)
			return p;
	}

	if (peak_buffer != nullptr) {
		const void *p = fifo_buffer_read(peak_buffer, length_r);
		if (p != nullptr)
			return p;
	}

	return nullptr;
}

void
PeakBuffer::Consume(size_t length)
{
	if (normal_buffer != nullptr && !fifo_buffer_is_empty(normal_buffer)) {
		fifo_buffer_consume(normal_buffer, length);
		return;
	}

	if (peak_buffer != nullptr && !fifo_buffer_is_empty(peak_buffer)) {
		fifo_buffer_consume(peak_buffer, length);
		if (fifo_buffer_is_empty(peak_buffer)) {
			HugeFree(peak_buffer, peak_size);
			peak_buffer = nullptr;
		}

		return;
	}
}

static size_t
AppendTo(fifo_buffer *buffer, const void *data, size_t length)
{
	assert(data != nullptr);
	assert(length > 0);

	size_t total = 0;

	do {
		size_t max_length;
		void *p = fifo_buffer_write(buffer, &max_length);
		if (p == nullptr)
			break;

		const size_t nbytes = std::min(length, max_length);
		memcpy(p, data, nbytes);
		fifo_buffer_append(buffer, nbytes);

		data = (const uint8_t *)data + nbytes;
		length -= nbytes;
		total += nbytes;
	} while (length > 0);

	return total;
}

bool
PeakBuffer::Append(const void *data, size_t length)
{
	if (length == 0)
		return true;

	if (peak_buffer != nullptr && !fifo_buffer_is_empty(peak_buffer)) {
		size_t nbytes = AppendTo(peak_buffer, data, length);
		return nbytes == length;
	}

	if (normal_buffer == nullptr)
		normal_buffer = fifo_buffer_new(normal_size);

	size_t nbytes = AppendTo(normal_buffer, data, length);
	if (nbytes > 0) {
		data = (const uint8_t *)data + nbytes;
		length -= nbytes;
		if (length == 0)
			return true;
	}

	if (peak_buffer == nullptr) {
		if (peak_size > 0)
			peak_buffer = (fifo_buffer *)HugeAllocate(peak_size);
		if (peak_buffer == nullptr)
			return false;

		fifo_buffer_init(peak_buffer, peak_size);
	}

	nbytes = AppendTo(peak_buffer, data, length);
	return nbytes == length;
}
