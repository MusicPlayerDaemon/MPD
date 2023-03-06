// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "MD5.hxx"
#include "Hash.hxx"

namespace Gcrypt {

std::array<std::byte, 16>
MD5(std::span<const std::byte> input) noexcept
{
	return Gcrypt::Hash<GCRY_MD_MD5, 16>(input);
}

} // namespace Gcrypt
