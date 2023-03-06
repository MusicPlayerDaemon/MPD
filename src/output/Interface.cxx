// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Interface.hxx"

#include <stdexcept>

void
AudioOutput::SetAttribute([[maybe_unused]] std::string &&name,
			  [[maybe_unused]] std::string &&value)
{
	throw std::invalid_argument("Unsupported attribute");
}
