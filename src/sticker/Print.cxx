// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Print.hxx"
#include "Sticker.hxx"
#include "client/Response.hxx"

#include <fmt/format.h>

void
sticker_print_value(Response &r,
		    const char *name, const char *value)
{
	r.Fmt(FMT_STRING("sticker: {}={}\n"), name, value);
}

void
sticker_print(Response &r, const Sticker &sticker)
{
	for (const auto &[name, val] : sticker.table)
		sticker_print_value(r, name.c_str(), val.c_str());
}
