// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Implementation of D. J. Bernstein's cdb hash function.
 * http://cr.yp.to/cdb/cdb.txt
 */

#pragma once

#include <cstddef>
#include <span>

static constexpr std::size_t DJB_HASH_INIT = 5381;

[[gnu::pure]]
std::size_t
djb_hash(std::span<const std::byte> src,
	 std::size_t init=DJB_HASH_INIT) noexcept;

[[gnu::pure]]
std::size_t
djb_hash_string(const char *p,
		std::size_t init=DJB_HASH_INIT) noexcept;
