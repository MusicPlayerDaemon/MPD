/*
 * Copyright 2020-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_WIN32_HRESULT_HXX
#define MPD_WIN32_HRESULT_HXX

#include "util/Compiler.h"

#include <string_view>
#include <system_error>

#include <windef.h>

gcc_const
std::string_view
HRESULTToString(HRESULT result) noexcept;

static inline const std::error_category &hresult_category() noexcept;
class HResultCategory : public std::error_category {
public:
	const char *name() const noexcept override { return "HRESULT"; }
	std::string message(int Errcode) const override;
	std::error_condition default_error_condition(int code) const noexcept override {
		return std::error_condition(code, hresult_category());
	}
};
static inline const std::error_category &hresult_category() noexcept {
	static const HResultCategory hresult_category_instance{};
	return hresult_category_instance;
}

inline std::system_error
MakeHResultError(HRESULT result, const char *msg) noexcept
{
	return std::system_error(std::error_code(result, hresult_category()),
				 msg);
}

gcc_printf(2, 3) std::system_error
FormatHResultError(HRESULT result, const char *fmt, ...) noexcept;

#endif
