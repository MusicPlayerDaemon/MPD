// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef> // for std::size_t

static constexpr std::size_t KIBI = 1024;
static constexpr std::size_t MEBI = 1024 * KIBI;
static constexpr std::size_t GIBI = 1024 * MEBI;

static constexpr std::size_t
operator""_KiB(unsigned long long n) noexcept
{
	return n * KIBI;
}

static constexpr std::size_t
operator""_MiB(unsigned long long n) noexcept
{
	return n * MEBI;
}

static constexpr std::size_t
operator""_GiB(unsigned long long n) noexcept
{
	return n * GIBI;
}
