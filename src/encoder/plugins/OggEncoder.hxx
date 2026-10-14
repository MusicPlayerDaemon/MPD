// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "../EncoderAPI.hxx"
#include "lib/xiph/OggStreamState.hxx"

/**
 * An abstract base class which contains code common to all encoders
 * with Ogg container output.
 */
class OggEncoder : public Encoder {
	/* initialize "flush" to true, so the caller gets the full
	   headers on the first read */
	bool flush = true;

protected:
	OggStreamState stream;

public:
	explicit OggEncoder(bool _implements_tag) noexcept;

	/* virtual methods from class Encoder */
	void Flush() final;
	std::span<const std::byte> Read(std::span<std::byte> buffer) noexcept override;
};
