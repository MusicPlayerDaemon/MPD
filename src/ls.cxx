// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "ls.hxx"
#include "input/Registry.hxx"
#include "input/InputPlugin.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "client/Response.hxx"
#include "util/UriExtract.hxx"

#include <fmt/format.h>

#include <cassert>
#include <string>

void print_supported_uri_schemes_to_fp(FILE *fp)
{
#ifdef HAVE_UN
	fmt::print(fp, " file://");
#endif
	std::set<std::string, std::less<>> protocols;
	input_plugins_for_each(plugin)
		plugin->ForeachSupportedUri([&](const char* uri) {
			protocols.emplace(uri);
		});

	decoder_plugins_for_each([&protocols](const auto &plugin){
		if (plugin.protocols != nullptr)
			protocols.merge(plugin.protocols());
	});

	for (const auto& protocol : protocols) {
		fmt::print(fp, " {}", protocol);
	}
	fmt::print(fp, "\n");
}

void
print_supported_uri_schemes(Response &r)
{
	std::set<std::string, std::less<>> protocols;
	input_plugins_for_each_enabled(plugin)
		plugin->ForeachSupportedUri([&](const char* uri) {
			protocols.emplace(uri);
		});

	decoder_plugins_for_each_enabled([&protocols](const auto &plugin){
		if (plugin.protocols != nullptr)
			protocols.merge(plugin.protocols());
	});

	for (const auto& protocol : protocols) {
		r.Fmt(FMT_STRING("handler: {}\n"), protocol);
	}
}

bool
uri_supported_scheme(const char *uri) noexcept
{
	assert(uri_has_scheme(uri));

	input_plugins_for_each_enabled(plugin)
		if (plugin->SupportsUri(uri))
			return true;

	return decoder_plugins_try([uri](const auto &plugin){
		return plugin.SupportsUri(uri);
	});
}
