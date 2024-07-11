// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "util/DereferenceIterator.hxx"
#include "util/FilteredContainer.hxx"
#include "util/TerminatedArray.hxx"

/**
 * NULL terminated list of all input plugins which were enabled at
 * compile time.
 */
extern const struct InputPlugin *const input_plugins[];

extern bool input_plugins_enabled[];

static inline auto
GetAllInputPlugins() noexcept
{
	return DereferenceContainerAdapter{TerminatedArray<const InputPlugin *const, nullptr>{input_plugins}};
}

static inline auto
GetEnabledInputPlugins() noexcept
{
	const auto all = GetAllInputPlugins();
	return FilteredContainer{all.begin(), all.end(), input_plugins_enabled};
}

[[gnu::pure]]
bool
HasRemoteTagScanner(const char *uri) noexcept;
