// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_WASAPI_PROPERTY_STORE_HXX
#define MPD_WASAPI_PROPERTY_STORE_HXX

#include "win32/PropVariant.hxx"
#include "util/AllocatedString.hxx"
#include "util/ScopeExit.hxx"

#include <propsys.h>

[[gnu::pure]]
inline AllocatedString
GetString(IPropertyStore &ps, REFPROPERTYKEY key) noexcept
{
	PROPVARIANT pv;
	PropVariantInit(&pv);

	HRESULT result = ps.GetValue(key, &pv);
	if (FAILED(result))
		return nullptr;

	AtScopeExit(&) { PropVariantClear(&pv); };
	return ToString(pv);
}

#endif
