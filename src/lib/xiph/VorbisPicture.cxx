// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "VorbisPicture.hxx"
#include "lib/crypto/Base64.hxx"
#include "tag/Id3Picture.hxx"
#include "util/AllocatedArray.hxx"
#include "config.h"

void
ScanVorbisPicture(std::string_view value, TagHandler &handler) noexcept
{
#ifdef HAVE_BASE64
	if (value.size() > 1024 * 1024)
		/* ignore image files which are too huge */
		return;

	try {
		return ScanId3Apic(DecodeBase64(value), handler);
	} catch (...) {
		// TODO: log?
	}
#else
	(void)value;
	(void)handler;
#endif
}
