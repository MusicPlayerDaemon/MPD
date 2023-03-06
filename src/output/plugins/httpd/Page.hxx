// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PAGE_HXX
#define MPD_PAGE_HXX

#include "util/AllocatedArray.hxx"

#include <cstddef>
#include <memory>

/**
 * A dynamically allocated buffer.  It is used to pass
 * reference-counted buffers around (using std::shared_ptr), when
 * several instances hold references to one buffer.
 */
using Page = AllocatedArray<std::byte>;

typedef std::shared_ptr<Page> PagePtr;

#endif
