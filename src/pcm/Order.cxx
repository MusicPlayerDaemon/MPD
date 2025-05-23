// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Order.hxx"
#include "Buffer.hxx"
#include "util/SpanCast.hxx"

#include <utility> // for std::unreachable()

/*
 * According to:
 *  - https://xiph.org/flac/format.html#frame_header
 *  - https://github.com/nu774/qaac/wiki/Multichannel--handling
 * the source channel order (after decoding, e.g., flac, alac) is for
 *  - 1ch:            mono
 *  - 2ch:            left, right
 *  - 3ch:            left, right, center
 *  - 4ch:            front left, front right, back left, back right
 *  - 5ch:            front left, front right, front center, back/surround left, back/surround right
 *  - 6ch (aka 5.1):  front left, front right, front center, LFE, back/surround left, back/surround right
 *  - 7ch:            front left, front right, front center, LFE, back center, side left, side right
 *  - 8ch: (aka 7.1): front left, front right, front center, LFE, back left, back right, side left, side right
 *
 * The ALSA default channel map is (see /usr/share/alsa/pcm/surround71.conf):
 *  - front left, front right, back left, back right, front center, LFE,  side left, side right
 *
 * Hence, in case of the following source channel orders 3ch, 5ch, 6ch (aka
 * 5.1), 7ch and 8ch the channel order has to be adapted
 */

template<typename V>
struct TwoPointers {
	V *dest;
	const V *src;

	TwoPointers<V> &CopyOne() noexcept {
		*dest++ = *src++;
		return *this;
	}

	TwoPointers<V> &CopyTwo() noexcept {
		return CopyOne().CopyOne();
	}

	TwoPointers<V> &SwapTwoPairs() noexcept {
		*dest++ = src[2];
		*dest++ = src[3];
		*dest++ = src[0];
		*dest++ = src[1];
		src += 4;
		return *this;
	}

	TwoPointers<V> &ToAlsa50() noexcept {
		*dest++ = src[0]; // front left
		*dest++ = src[1]; // front right
		*dest++ = src[3]; // surround left
		*dest++ = src[4]; // surround right
		*dest++ = src[2]; // front center
		src += 5;
		return *this;
	}

	TwoPointers<V> &ToAlsa51() noexcept {
		return CopyTwo() // left+right
			.SwapTwoPairs(); // center, LFE, surround left+right
	}

	TwoPointers<V> &ToAlsa70() noexcept {
		*dest++ = src[0]; // front left
		*dest++ = src[1]; // front right
		*dest++ = src[5]; // side left
		*dest++ = src[6]; // side right
		*dest++ = src[2]; // front center
		*dest++ = src[3]; // LFE
		*dest++ = src[4]; // back center
		src += 7;
		return *this;
	}

	TwoPointers<V> &ToAlsa71() noexcept {
		return ToAlsa51()
			.CopyTwo(); // side left+right
	}
};

template<typename V>
static void
ToAlsaChannelOrder50(V *dest, const V *src, size_t n) noexcept
{
	TwoPointers<V> p{dest, src};
	for (size_t i = 0; i != n; ++i)
		p.ToAlsa50();
}

template<typename V>
static inline std::span<const V>
ToAlsaChannelOrder50(PcmBuffer &buffer, std::span<const V> src) noexcept
{
	auto dest = buffer.GetT<V>(src.size());
	ToAlsaChannelOrder50(dest, src.data(), src.size() / 5);
	return { dest, src.size() };
}

template<typename V>
static void
ToAlsaChannelOrder51(V *dest, const V *src, size_t n) noexcept
{
	TwoPointers<V> p{dest, src};
	for (size_t i = 0; i != n; ++i)
		p.ToAlsa51();
}

template<typename V>
static inline std::span<const V>
ToAlsaChannelOrder51(PcmBuffer &buffer, std::span<const V> src) noexcept
{
	auto dest = buffer.GetT<V>(src.size());
	ToAlsaChannelOrder51(dest, src.data(), src.size() / 6);
	return { dest, src.size() };
}

template<typename V>
static void
ToAlsaChannelOrder70(V *dest, const V *src, size_t n) noexcept
{
	TwoPointers<V> p{dest, src};
	for (size_t i = 0; i != n; ++i)
		p.ToAlsa70();
}

template<typename V>
static inline std::span<const V>
ToAlsaChannelOrder70(PcmBuffer &buffer, std::span<const V> src) noexcept
{
	auto dest = buffer.GetT<V>(src.size());
	ToAlsaChannelOrder70(dest, src.data(), src.size() / 7);
	return { dest, src.size() };
}

template<typename V>
static void
ToAlsaChannelOrder71(V *dest, const V *src, size_t n) noexcept
{
	TwoPointers<V> p{dest, src};
	for (size_t i = 0; i != n; ++i)
		p.ToAlsa71();
}

template<typename V>
static inline std::span<const V>
ToAlsaChannelOrder71(PcmBuffer &buffer, std::span<const V> src) noexcept
{
	auto dest = buffer.GetT<V>(src.size());
	ToAlsaChannelOrder71(dest, src.data(), src.size() / 8);
	return { dest, src.size() };
}

template<typename V>
static std::span<const V>
ToAlsaChannelOrderT(PcmBuffer &buffer, std::span<const V> src,
		    unsigned channels) noexcept
{
	switch (channels) {
	case 5: // 5.0
		return ToAlsaChannelOrder50(buffer, src);

	case 6: // 5.1
		return ToAlsaChannelOrder51(buffer, src);

	case 7: // 7.0
		return ToAlsaChannelOrder70(buffer, src);

	case 8: // 7.1
		return ToAlsaChannelOrder71(buffer, src);

	default:
		return src;
	}
}

std::span<const std::byte>
ToAlsaChannelOrder(PcmBuffer &buffer, std::span<const std::byte> src,
		   SampleFormat sample_format, unsigned channels) noexcept
{
	switch (sample_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::S8:
	case SampleFormat::DSD:
		return src;

	case SampleFormat::S16:
		return std::as_bytes(ToAlsaChannelOrderT(buffer,
							 FromBytesStrict<const int16_t>(src),
							 channels));

	case SampleFormat::S24_P32:
	case SampleFormat::S32:
	case SampleFormat::FLOAT:
		return std::as_bytes(ToAlsaChannelOrderT(buffer,
							 FromBytesStrict<const int32_t>(src),
							 channels));
	}

	std::unreachable();
}
