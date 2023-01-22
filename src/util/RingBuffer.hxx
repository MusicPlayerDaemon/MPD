/*
 * Copyright 2023 Max Kellermann <max.kellermann@gmail.com>
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

#pragma once

#include "AllocatedArray.hxx"

#include <atomic>
#include <cassert>
#include <type_traits>

/**
 * A thread-safe (single-producer, single-consumer; lock-free and
 * wait-free) circular buffer.
 *
 * This implementation is optimized for bulk read/write
 * (i.e. producing and consuming more than one item at a time).
 */
template<typename T>
requires std::is_trivial_v<T>
class RingBuffer {
protected:
	AllocatedArray<T> buffer;

	std::atomic_size_t write_position{0}, read_position{0};

public:
	/**
	 * This default constructor will not allocate a buffer.
	 * IsDefined() will return false; it is not usable.  To
	 * allocate a buffer later, create a new instance and use the
	 * move operator.
	 */
	RingBuffer() noexcept = default;

	/**
	 * Allocate a buffer of the specified size.
	 *
	 * The actual allocation will be the specified #capacity plus
	 * one, because for internal management, one slot needs to
	 * stay empty.
	 */
	explicit RingBuffer(std::size_t capacity) noexcept
		:buffer(capacity + 1) {}

	/**
	 * Move the allocated buffer from another instance.
	 *
	 * This operation is not thread-safe.
	 */
	RingBuffer(RingBuffer &&src) noexcept
		:buffer(std::move(src.buffer)),
		 write_position(src.write_position.load(std::memory_order_relaxed)),
		 read_position(src.read_position.load(std::memory_order_relaxed))
	{
	}

	/**
	 * Move the allocated buffer from another instance.
	 *
	 * This operation is not thread-safe.
	 */
	RingBuffer &operator=(RingBuffer &&src) noexcept {
		buffer = std::move(src.buffer);
		write_position = src.write_position.load(std::memory_order_relaxed);
		read_position = src.read_position.load(std::memory_order_relaxed);
		return *this;
	}

	/**
	 * Was a buffer allocated?
	 */
	bool IsDefined() const noexcept {
		return buffer.capacity() > 0;
	}

	/**
	 * Discard the contents of this buffer.
	 *
	 * This method is not thread-safe.  For a thread-safe version,
	 * use Discard().
	 */
	void Clear() noexcept {
		assert(!IsDefined() || read_position.load() < buffer.capacity());
		assert(!IsDefined() || write_position.load() < buffer.capacity());

		write_position.store(0, std::memory_order_relaxed);
		read_position.store(0, std::memory_order_relaxed);
	}

	bool IsFull() const noexcept {
		const auto rp = GetPreviousIndex(read_position.load(std::memory_order_relaxed));
		const auto wp = write_position.load(std::memory_order_relaxed);
		return rp == wp;
	}

	/**
	 * Prepare a contiguous write directly into the buffer.  The
	 * returned span (which, of course, cannot wrap around the end
	 * of the ring) may be written to; after that, call Append()
	 * to commit the write.
	 */
	[[gnu::pure]]
	std::span<T> Write() noexcept {
		assert(IsDefined());

		const auto wp = write_position.load(std::memory_order_acquire);
		assert(wp < buffer.capacity());

		const auto rp = GetPreviousIndex(read_position.load(std::memory_order_relaxed));
		assert(rp < buffer.capacity());

		std::size_t n = (wp <= rp ? rp : buffer.capacity()) - wp;
		return {&buffer[wp], n};
	}

	/**
	 * Commit the write prepared by Write().
	 */
	void Append(std::size_t n) noexcept {
		Add(write_position, n);
	}

	/**
	 * Determine how many items may be written.  This considers
	 * wraparound.
	 */
	[[gnu::pure]]
	std::size_t WriteAvailable() const noexcept {
		assert(IsDefined());

		const auto wp = write_position.load(std::memory_order_relaxed);
		const auto rp = GetPreviousIndex(read_position.load(std::memory_order_relaxed));

		return wp <= rp
			? rp - wp
			: buffer.capacity() - wp + rp;
	}

	/**
	 * Append data from the given span to this buffer, handling
	 * wraparound.
	 *
	 * @return the number of items appended
	 */
	std::size_t WriteFrom(std::span<const T> src) noexcept {
		auto wp = write_position.load(std::memory_order_acquire);
		const auto rp = GetPreviousIndex(read_position.load(std::memory_order_relaxed));

		std::size_t n = std::min((wp <= rp ? rp : buffer.capacity()) - wp,
					 src.size());
		CopyFrom(wp, src.first(n));

		wp += n;
		if (wp >= buffer.capacity()) {
			// wraparound
			src = src.subspan(n);
			wp = std::min(rp, src.size());
			CopyFrom(0, src.first(wp));
			n += wp;
		}

		write_position.store(wp, std::memory_order_release);

		return n;
	}

	/**
	 * Like WriteFrom(), but ensure to never copy partial
	 * "frames"; a frame being a fixed-size group of items.
	 *
	 * @param frame_size the number of items which form one frame;
	 * the return value of this function is always a multiple of
	 * this value
	 */
	std::size_t WriteFramesFrom(std::span<const T> src, std::size_t frame_size) noexcept {
		// TODO optimize, eliminate duplicate atomic reads

		std::size_t available = WriteAvailable();
		std::size_t frames_available = available / frame_size;
		std::size_t rounded_available = frames_available * frame_size;

		if (rounded_available < src.size())
			src = src.first(rounded_available);
		
		return WriteFrom(src);
	}

	/**
	 * Prepare a contiguous read directly from the buffer.  The
	 * returned span (which, of course, cannot wrap around the end
	 * of the ring) may be read from; after that, call Consume()
	 * to commit the read.
	 */
	[[gnu::pure]]
	std::span<const T> Read() const noexcept {
		const auto rp = read_position.load(std::memory_order_acquire);
		const auto wp = write_position.load(std::memory_order_relaxed);

		std::size_t n = (rp <= wp ? wp : buffer.capacity()) - rp;
		return {&buffer[rp], n};
	}

	/**
	 * Commit the read prepared by Read().
	 */
	void Consume(std::size_t n) noexcept {
		Add(read_position, n);
	}

	/**
	 * Determine how many items may be read.  This considers
	 * wraparound.
	 */
	[[gnu::pure]]
	std::size_t ReadAvailable() const noexcept {
		assert(IsDefined());

		const auto rp = read_position.load(std::memory_order_relaxed);
		const auto wp = write_position.load(std::memory_order_relaxed);

		return rp <= wp
			? wp - rp
			: buffer.capacity() - rp + wp;
	}

	/**
	 * Pop data from this buffer to the given span, handling
	 * wraparound.
	 *
	 * @return the number of items move to the span
	 */
	std::size_t ReadTo(std::span<T> dest) noexcept {
		auto rp = read_position.load(std::memory_order_acquire);
		const auto wp = write_position.load(std::memory_order_relaxed);

		std::size_t n = std::min((rp <= wp ? wp : buffer.capacity()) - rp,
					 dest.size());
		CopyTo(rp, dest.first(n));

		rp += n;
		if (rp >= buffer.capacity()) {
			// wraparound
			dest = dest.subspan(n);
			rp = std::min(wp, dest.size());
			CopyTo(0, dest.first(rp));
			n += rp;
		}

		read_position.store(rp, std::memory_order_release);

		return n;
	}

	/**
	 * Like WriteFrom(), but ensure to never copy partial
	 * "frames"; a frame being a fixed-size group of items.
	 *
	 * @param frame_size the number of items which form one frame;
	 * the return value of this function is always a multiple of
	 * this value
	 */
	std::size_t ReadFramesTo(std::span<T> dest, std::size_t frame_size) noexcept {
		// TODO optimize, eliminate duplicate atomic reads

		std::size_t available = ReadAvailable();
		std::size_t frames_available = available / frame_size;
		std::size_t rounded_available = frames_available * frame_size;

		if (rounded_available < dest.size())
			dest = dest.first(rounded_available);
		
		return ReadTo(dest);
	}

	/**
	 * Discard the contents of this buffer.
	 *
	 * This method is thread-safe, but it may only be called from
	 * the consumer thread.
	 */
	void Discard() noexcept {
		const auto wp = write_position.load(std::memory_order_relaxed);
		read_position.store(wp, std::memory_order_release);
	}

private:
	[[gnu::const]]
	std::size_t GetPreviousIndex(std::size_t i) const noexcept {
		assert(IsDefined());

		if (i == 0)
			i = buffer.capacity();
		return --i;
	}

	void Add(std::atomic_size_t &dest, std::size_t n) const noexcept {
		assert(IsDefined());

		const std::size_t old_value = dest.load(std::memory_order_acquire);
		assert(old_value < buffer.capacity());

		std::size_t new_value = old_value + n;
		assert(new_value <= buffer.capacity());
		if (new_value >= buffer.capacity())
			new_value = 0;
		dest.store(new_value, std::memory_order_release);
	}

	void CopyFrom(std::size_t dest_position, std::span<const T> src) noexcept {
		std::copy(src.begin(), src.end(), &buffer[dest_position]);
	}

	void CopyTo(std::size_t src_position, std::span<T> dest) noexcept {
		std::copy_n(&buffer[src_position], dest.size(), dest.begin());
	}
};
