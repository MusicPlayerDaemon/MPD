// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
