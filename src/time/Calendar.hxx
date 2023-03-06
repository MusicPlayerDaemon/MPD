// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

constexpr bool
IsLeapYear(unsigned y) noexcept
{
	return (y % 4) == 0 && ((y % 100) != 0 || (y % 400) == 0);
}

constexpr unsigned
DaysInFebruary(unsigned year) noexcept
{
	return IsLeapYear(year) ? 29 : 28;
}

constexpr unsigned
DaysInMonth(unsigned month, unsigned year) noexcept
{
	if (month == 4 || month == 6 || month == 9 || month == 11)
		return 30;
	else if (month != 2)
		return 31;
	else
		return DaysInFebruary(year);
}

constexpr unsigned
DaysInYear(unsigned year) noexcept
{
	return IsLeapYear(year) ? 366 : 365;
}
