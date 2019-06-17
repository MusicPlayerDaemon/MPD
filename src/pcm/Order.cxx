/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "Order.hxx"
#include "Buffer.hxx"
#include "util/ConstBuffer.hxx"

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

	TwoPointers<V> &ToAlsa51() noexcept {
		return CopyTwo() // left+right
			.SwapTwoPairs(); // center, LFE, surround left+right
	}

	TwoPointers<V> &ToAlsa71() noexcept {
		return ToAlsa51()
			.CopyTwo(); // side left+right
	}
};

template<typename V>
static void
ToAlsaChannelOrder51(V *dest, const V *src, size_t n) noexcept
{
	TwoPointers<V> p{dest, src};
	for (size_t i = 0; i != n; ++i)
		p.ToAlsa51();
}

template<typename V>
static inline ConstBuffer<V>
ToAlsaChannelOrder51(PcmBuffer &buffer, ConstBuffer<V> src) noexcept
{
	auto dest = buffer.GetT<V>(src.size);
	ToAlsaChannelOrder51(dest, src.data, src.size / 6);
	return { dest, src.size };
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
static inline ConstBuffer<V>
ToAlsaChannelOrder71(PcmBuffer &buffer, ConstBuffer<V> src) noexcept
{
	auto dest = buffer.GetT<V>(src.size);
	ToAlsaChannelOrder71(dest, src.data, src.size / 8);
	return { dest, src.size };
}

template<typename V>
static ConstBuffer<V>
ToAlsaChannelOrderT(PcmBuffer &buffer, ConstBuffer<V> src,
		    unsigned channels) noexcept
{
	switch (channels) {
	case 6: // 5.1
		return ToAlsaChannelOrder51(buffer, src);

	case 8: // 7.1
		return ToAlsaChannelOrder71(buffer, src);

	default:
		return src;
	}
}

ConstBuffer<void>
ToAlsaChannelOrder(PcmBuffer &buffer, ConstBuffer<void> src,
		   SampleFormat sample_format, unsigned channels) noexcept
{
	switch (sample_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::S8:
	case SampleFormat::DSD:
		return src;

	case SampleFormat::S16:
		return ToAlsaChannelOrderT(buffer,
					   ConstBuffer<int16_t>::FromVoid(src),
					   channels).ToVoid();

	case SampleFormat::S24_P32:
	case SampleFormat::S32:
	case SampleFormat::FLOAT:
		return ToAlsaChannelOrderT(buffer,
					   ConstBuffer<int32_t>::FromVoid(src),
					   channels).ToVoid();
	}

	gcc_unreachable();
}
