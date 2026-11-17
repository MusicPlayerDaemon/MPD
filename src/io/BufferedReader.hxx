// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/ByteSizes.hxx"
#include "util/DynamicFifoBuffer.hxx"

#include <cstddef>
#include <span>
#include <string_view>

class Reader;

class BufferedReader {
	static constexpr std::size_t MAX_SIZE = 512_KiB;

	Reader &reader;

	DynamicFifoBuffer<std::byte> buffer{16_KiB};

	bool eof = false;

	unsigned line_number = 0;

public:
	explicit BufferedReader(Reader &_reader) noexcept
		:reader(_reader) {}

	/**
	 * Reset the internal state.  Should be called after rewinding
	 * the underlying #Reader.
	 */
	void Reset() noexcept {
		buffer.Clear();
		eof = false;
		line_number = 0;
	}

	bool Fill(bool need_more);

	[[gnu::pure]]
	std::span<std::byte> Read() const noexcept {
		return std::as_writable_bytes(buffer.Read());
	}

	/**
	 * Read a buffer of exactly the given size (without consuming
	 * it).  Throws std::runtime_error if not enough data is
	 * available.
	 */
	void *ReadFull(std::size_t size);

	void Consume(std::size_t n) noexcept {
		buffer.Consume(n);
	}

	/**
	 * Read (and consume) data from the input buffer into the
	 * given buffer.  Does not attempt to refill the buffer.
	 */
	std::size_t ReadFromBuffer(std::span<std::byte> dest) noexcept;

	/**
	 * Read data into the given buffer and consume it from our
	 * buffer.  Throw an exception if the request cannot be
	 * forfilled.
	 */
	void ReadFull(std::span<std::byte> dest);

	template<typename T>
	void ReadFullT(T &dest) {
		ReadFull(std::as_writable_bytes(std::span{&dest, 1}));
	}

	template<typename T>
	T ReadFullT() {
		T dest;
		ReadFullT<T>(dest);
		return dest;
	}

	char *ReadLine();

	/**
	 * Like ReadLine(), but return a std::string_view.  On end of
	 * file, a zero-initialized std::string_view is returned.
	 */
	std::string_view ReadLineView();

	unsigned GetLineNumber() const noexcept {
		return line_number;
	}
};
