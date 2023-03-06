// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef YAJL_PARSE_INPUT_STREAM_HXX
#define YAJL_PARSE_INPUT_STREAM_HXX

class InputStream;

namespace Yajl {

class Handle;

void
ParseInputStream(Handle &handle, InputStream &is);

} // namespace Yajl

#endif
