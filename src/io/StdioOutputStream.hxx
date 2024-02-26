// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "OutputStream.hxx"

#include <stdio.h>

class StdioOutputStream final : public OutputStream {
	FILE *const file;

public:
	explicit StdioOutputStream(FILE *_file) noexcept:file(_file) {}

	/* virtual methods from class OutputStream */
	void Write(std::span<const std::byte> src) override {
		fwrite(src.data(), 1, src.size(), file);

		/* this class is debug-only and ignores errors */
	}
};
