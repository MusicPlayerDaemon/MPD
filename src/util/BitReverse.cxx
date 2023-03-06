// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "BitReverse.hxx"

static constexpr BitReverseTable
GenerateBitReverseTable() noexcept
{
	BitReverseTable table{};
	for (unsigned i = 0; i < 256; ++i)
		table.data[i] = BitReverseMultiplyModulus(static_cast<std::byte>(i));
	return table;
}

const BitReverseTable bit_reverse_table = GenerateBitReverseTable();
