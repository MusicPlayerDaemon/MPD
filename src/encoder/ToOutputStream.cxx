// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ToOutputStream.hxx"
#include "EncoderInterface.hxx"
#include "io/OutputStream.hxx"
#include "util/ByteSizes.hxx"

void
EncoderToOutputStream(OutputStream &os, Encoder &encoder)
{
	while (true) {
		/* read from the encoder */

		std::byte buffer[32_KiB];
		const auto r = encoder.Read(buffer);
		if (r.empty())
			return;

		/* write everything to the stream */

		os.Write(r);
	}
}
