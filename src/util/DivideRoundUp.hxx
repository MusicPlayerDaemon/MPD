// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <concepts> // for std::unsigned_integral

template<std::unsigned_integral T>
static constexpr T
DivideRoundUp(T a, T b) noexcept
{
	return (a + b - 1) / b;
}
