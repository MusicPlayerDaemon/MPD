// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ScanTags.hxx"
#include "RemoteTagScanner.hxx"
#include "InputPlugin.hxx"
#include "Registry.hxx"

std::unique_ptr<RemoteTagScanner>
InputScanTags(const char *uri, RemoteTagHandler &handler)
{
	input_plugins_for_each_enabled(plugin) {
		if (plugin->scan_tags == nullptr || !plugin->SupportsUri(uri))
			continue;

		auto scanner = plugin->scan_tags(uri, handler);
		if (scanner)
			return scanner;
	}

	/* unsupported URI */
	return nullptr;
}
