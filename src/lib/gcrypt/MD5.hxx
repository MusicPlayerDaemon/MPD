// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <array>
#include <span>

namespace Gcrypt {

[[gnu::pure]]
std::array<std::byte, 16>
MD5(std::span<const std::byte> input) noexcept;

} // namespace Gcrypt
