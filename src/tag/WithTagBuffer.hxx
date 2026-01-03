// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Tag.hxx"

/**
 * Helper class for #ExportedSong and #ProxySong so the #Tag field
 * gets initialized before the #LightSong base class.  This
 * initialization order is needed so #LightSong is allowed to refer to
 * the #Tag field.
 */
struct WithTagBuffer {
	/**
	 * A reference target for LightSong::tag, but it is only used
	 * if this instance "owns" the #Tag.  For instances referring
	 * to a foreign #Tag instance (e.g. a Song::tag), this field
	 * is not used (and empty).
	 */
	Tag tag_buffer;

	WithTagBuffer() noexcept = default;

	WithTagBuffer(Tag &&src) noexcept
		:tag_buffer(std::move(src)) {}
};
