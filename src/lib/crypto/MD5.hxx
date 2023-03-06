// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <array>
#include <span>

void
GlobalInitMD5() noexcept;

[[gnu::pure]]
std::array<std::byte, 16>
MD5(std::span<const std::byte> input) noexcept;

[[gnu::pure]]
std::array<char, 32>
MD5Hex(std::span<const std::byte> input) noexcept;
