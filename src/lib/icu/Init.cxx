// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Init.hxx"
#include "Collate.hxx"
#include "Canonicalize.hxx"
#include "Error.hxx"

#include <unicode/uclean.h>

void
IcuInit()
{
	UErrorCode code = U_ZERO_ERROR;
	u_init(&code);
	if (U_FAILURE(code))
		throw ICU::MakeError(code, "u_init() failed");

	IcuCollateInit();
	IcuCanonicalizeInit();
}

void
IcuFinish() noexcept
{
	IcuCanonicalizeFinish();
	IcuCollateFinish();

	u_cleanup();
}
