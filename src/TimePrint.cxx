// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "TimePrint.hxx"
#include "client/Response.hxx"
#include "time/ISO8601.hxx"
#include "util/StringBuffer.hxx"

#include <fmt/format.h>

void
time_print(Response &r, const char *name,
	   std::chrono::system_clock::time_point t)
{
	StringBuffer<64> s;

	try {
		s = FormatISO8601(t);
	} catch (...) {
		return;
	}

	r.Fmt(FMT_STRING("{}: {}\n"), name, s);
}
