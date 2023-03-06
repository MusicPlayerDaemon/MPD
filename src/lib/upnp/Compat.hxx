// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_UPNP_COMPAT_HXX
#define MPD_UPNP_COMPAT_HXX

#ifdef __clang__
/* libupnp versions until 1.10.1 redefine "bool" and "true" */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wkeyword-macro"

/* libupnp 1.8.4 uses a flawed kludge to suppress this warning in
   inline function __list_add_valid() */
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#endif

#include <upnp.h>

#ifdef __clang__
#pragma GCC diagnostic pop
#endif

#endif
