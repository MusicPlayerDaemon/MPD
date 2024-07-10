// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DecoderPrint.hxx"
#include "DecoderList.hxx"
#include "DecoderPlugin.hxx"
#include "client/Response.hxx"

#include <fmt/format.h>

#include <cassert>
#include <functional>

static void
decoder_plugin_print(Response &r,
		     const DecoderPlugin &plugin)
{
	const char *const*p;

	assert(plugin.name != nullptr);

	r.Fmt(FMT_STRING("plugin: {}\n"), plugin.name);

	if (plugin.suffixes != nullptr)
		for (p = plugin.suffixes; *p != nullptr; ++p)
			r.Fmt(FMT_STRING("suffix: {}\n"), *p);

	if (plugin.suffixes_function != nullptr)
		for (const auto &i : plugin.suffixes_function())
			r.Fmt(FMT_STRING("suffix: {}\n"), i);

	if (plugin.mime_types != nullptr)
		for (p = plugin.mime_types; *p != nullptr; ++p)
			r.Fmt(FMT_STRING("mime_type: {}\n"), *p);
}

void
decoder_list_print(Response &r)
{
	for (const auto &plugin : GetEnabledDecoderPlugins())
		decoder_plugin_print(r, plugin);
}
