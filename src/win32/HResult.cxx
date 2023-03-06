// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifdef _WIN32
// COM needs the "MSG" typedef, and audiopolicy.h includes COM headers
#undef NOUSER
#endif

#include "HResult.hxx"

#include <fmt/core.h>

#include <cassert>

#include <combaseapi.h> // needed by audiopolicy.h if COM_NO_WINDOWS_H is defined
#include <audiopolicy.h>

std::string_view
HRESULTToString(HRESULT result) noexcept
{
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

std::string
HResultCategory::message(int Errcode) const
{
	char buffer[256];

	/* FormatMessage() supports some HRESULT values (depending on
	   the Windows version) */
	if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
			   FORMAT_MESSAGE_IGNORE_INSERTS,
			   nullptr, Errcode, 0,
			   buffer, sizeof(buffer),
			   nullptr))
		return buffer;

	const auto msg = HRESULTToString(Errcode);
	if (!msg.empty())
		return std::string(msg);

	return fmt::format("{:#x}", Errcode);
}
