// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_WIN32_HRESULT_HXX
#define MPD_WIN32_HRESULT_HXX

#include <string_view>
#include <system_error>

#include <windef.h>

[[gnu::const]]
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

#endif
