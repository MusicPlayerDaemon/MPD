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

#include <audiopolicy.h>

constexpr std::string_view HRESULTToString(HRESULT result) {
	using namespace std::literals;
	switch (result) {
#define C(x)                                                                             \
case x:                                                                                  \
	return #x##sv
		C(AUDCLNT_E_ALREADY_INITIALIZED);
		C(AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL);
		C(AUDCLNT_E_BUFFER_ERROR);
		C(AUDCLNT_E_BUFFER_OPERATION_PENDING);
		C(AUDCLNT_E_BUFFER_SIZE_ERROR);
		C(AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED);
		C(AUDCLNT_E_BUFFER_TOO_LARGE);
		C(AUDCLNT_E_CPUUSAGE_EXCEEDED);
		C(AUDCLNT_E_DEVICE_INVALIDATED);
		C(AUDCLNT_E_DEVICE_IN_USE);
		C(AUDCLNT_E_ENDPOINT_CREATE_FAILED);
		C(AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED);
		C(AUDCLNT_E_INVALID_DEVICE_PERIOD);
		C(AUDCLNT_E_OUT_OF_ORDER);
		C(AUDCLNT_E_SERVICE_NOT_RUNNING);
		C(AUDCLNT_E_UNSUPPORTED_FORMAT);
		C(AUDCLNT_E_WRONG_ENDPOINT_TYPE);
		C(AUDCLNT_E_NOT_INITIALIZED);
		C(AUDCLNT_E_NOT_STOPPED);
		C(CO_E_NOTINITIALIZED);
		C(E_INVALIDARG);
		C(E_OUTOFMEMORY);
		C(E_POINTER);
		C(NO_ERROR);
#undef C
	}
	return std::string_view();
}

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
