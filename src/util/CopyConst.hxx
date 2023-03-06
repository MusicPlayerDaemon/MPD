// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <type_traits>

/**
 * Generate a type based on #To with the same const-ness as #From.
 */
template<typename To, typename From>
using CopyConst = std::conditional_t<std::is_const_v<From>,
				     std::add_const_t<To>,
				     std::remove_const_t<To>>;
