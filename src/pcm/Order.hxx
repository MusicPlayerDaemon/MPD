// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_ORDER_HXX
#define MPD_PCM_ORDER_HXX

#include "SampleFormat.hxx"

#include <span>

class PcmBuffer;

/**
 * Convert the given buffer from FLAC channel order
 * (https://xiph.org/flac/format.html) to ALSA channel order.
 */
std::span<const std::byte>
ToAlsaChannelOrder(PcmBuffer &buffer, std::span<const std::byte> src,
		   SampleFormat sample_format, unsigned channels) noexcept;

#endif
