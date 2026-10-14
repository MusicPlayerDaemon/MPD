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
	/**
	 * The header and body of the current #ogg_page.  If empty,
	 * then #OggStreamState::PageOut() must be called to generate
	 * another page.
	 */
	std::span<const std::byte> page_header, page_body;

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
