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

#include "NullEncoderPlugin.hxx"
#include "../EncoderAPI.hxx"
#include "util/DynamicFifoBuffer.hxx"

class NullEncoder final : public Encoder {
	DynamicFifoBuffer<std::byte> buffer{8192};

public:
	NullEncoder()
		:Encoder(false) {}

	/* virtual methods from class Encoder */
	void Write(std::span<const std::byte> src) override {
		buffer.Append(src);
	}

	std::span<const std::byte> Read(std::span<std::byte> b) noexcept override {
		return b.first(buffer.Read(b.data(), b.size()));
	}
};

class PreparedNullEncoder final : public PreparedEncoder {
public:
	/* virtual methods from class PreparedEncoder */
	Encoder *Open(AudioFormat &) override {
		return new NullEncoder();
	}
};

static PreparedEncoder *
null_encoder_init([[maybe_unused]] const ConfigBlock &block)
{
	return new PreparedNullEncoder();
}

const EncoderPlugin null_encoder_plugin = {
	"null",
	null_encoder_init,
};
