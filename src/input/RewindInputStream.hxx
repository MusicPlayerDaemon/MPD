// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * A wrapper for an input_stream object which allows cheap buffered
 * rewinding.  This is useful while detecting the stream codec (let
 * each decoder plugin peek a portion from the stream).
 */

#ifndef MPD_REWIND_INPUT_STREAM_HXX
#define MPD_REWIND_INPUT_STREAM_HXX

#include "Ptr.hxx"

InputStreamPtr
input_rewind_open(InputStreamPtr is);

#endif
