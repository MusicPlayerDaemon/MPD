/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "db/plugins/simple/Directory.hxx"
#include "archive/ArchiveList.hxx"
#include "decoder/DecoderList.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "fs/Traits.hxx"

gcc_pure
static bool
HaveArchivePluginForFilename(const char *filename) noexcept
{
#ifdef ENABLE_ARCHIVE
	const char *suffix = PathTraitsUTF8::GetFilenameSuffix(filename);
	return suffix != nullptr &&
		archive_plugin_from_suffix(suffix) != nullptr;
#else
	(void)filename;
	return false;
#endif
}

gcc_pure
static bool
HaveContainerPluginForFilename(const char *filename) noexcept
{
	const char *suffix = PathTraitsUTF8::GetFilenameSuffix(filename);
	return suffix != nullptr &&
		// TODO: check if this plugin really supports containers
		decoder_plugins_supports_suffix(suffix);
}

gcc_pure
static bool
HavePlaylistPluginForFilename(const char *filename) noexcept
{
	const char *suffix = PathTraitsUTF8::GetFilenameSuffix(filename);
	if (suffix == nullptr)
		return false;

	const auto plugin = FindPlaylistPluginBySuffix(suffix);
	if (plugin == nullptr)
		return false;

	/* discard the special directory if the user disables the
	   plugin's "as_directory" setting */
	return GetPlaylistPluginAsFolder(*plugin);
}

bool
Directory::IsPluginAvailable() const noexcept
{
	switch (device) {
	case DEVICE_INARCHIVE:
		return HaveArchivePluginForFilename(GetName());

	case DEVICE_CONTAINER:
		return HaveContainerPluginForFilename(GetName());

	case DEVICE_PLAYLIST:
		return HavePlaylistPluginForFilename(GetName());

	default:
		return true;
	}
}
