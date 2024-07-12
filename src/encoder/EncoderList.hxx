// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "util/DereferenceIterator.hxx"
#include "util/TerminatedArray.hxx"

struct EncoderPlugin;

extern const EncoderPlugin *const encoder_plugins[];

static inline auto
GetAllEncoderPlugins() noexcept
{
	return DereferenceContainerAdapter{TerminatedArray<const EncoderPlugin *const, nullptr>{encoder_plugins}};
}

/**
 * Looks up an encoder plugin by its name.
 *
 * @param name the encoder name to look for
 * @return the encoder plugin with the specified name, or nullptr if none
 * was found
 */
const EncoderPlugin *
encoder_plugin_get(const char *name);
