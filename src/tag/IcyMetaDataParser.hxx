// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ICY_META_DATA_PARSER_HXX
#define MPD_ICY_META_DATA_PARSER_HXX

#include "lib/icu/Converter.hxx"
#include "tag/Tag.hxx"
#include "config.h"

#include <cstddef>
#include <memory>
#include <span>

class IcyMetaDataParser {
	size_t data_size = 0, data_rest;

	size_t meta_size, meta_position;
	char *meta_data;

#ifdef HAVE_ICU_CONVERTER
	std::unique_ptr<IcuConverter> icu_converter;
#endif

	std::unique_ptr<Tag> tag;

public:
	~IcyMetaDataParser() noexcept {
		Reset();
	}

#ifdef HAVE_ICU_CONVERTER
	/**
	 * Throws on error.
	 */
	void SetCharset(const char *charset);
#endif

	/**
	 * Initialize an enabled icy_metadata object with the specified
	 * data_size (from the icy-metaint HTTP response header).
	 */
	void Start(size_t _data_size) noexcept {
		data_size = data_rest = _data_size;
		meta_size = 0;
		tag = nullptr;
	}

	/**
	 * Resets the icy_metadata.  Call this after rewinding the stream.
	 */
	void Reset() noexcept;

	/**
	 * Checks whether the icy_metadata object is enabled.
	 */
	bool IsDefined() const noexcept {
		return data_size > 0;
	}

	/**
	 * Evaluates data.  Returns the number of bytes of normal data which
	 * can be read by the caller, but not more than "length".  If the
	 * return value is smaller than "length", the caller should invoke
	 * icy_meta().
	 */
	size_t Data(size_t length) noexcept;

	/**
	 * Reads metadata from the stream.  Returns the number of
	 * bytes consumed.  If the return value is smaller than
	 * "src.size()", the caller should invoke Data().
	 */
	std::size_t Meta(std::span<const std::byte> src) noexcept;

	/**
	 * Parse data and eliminate metadata.
	 *
	 * @return the number of data bytes remaining in the buffer
	 */
	size_t ParseInPlace(std::span<std::byte> buffer) noexcept;

	std::unique_ptr<Tag> ReadTag() noexcept {
		return std::exchange(tag, nullptr);
	}
};

#endif
