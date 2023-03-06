// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PROTOCOL_ARGPARSER_HXX
#define MPD_PROTOCOL_ARGPARSER_HXX

#include <cstdint>

struct RangeArg;
class SongTime;
class SignedSongTime;

uint32_t
ParseCommandArgU32(const char *s);

int
ParseCommandArgInt(const char *s, int min_value, int max_value);

int
ParseCommandArgInt(const char *s);

RangeArg
ParseCommandArgRange(const char *s);

unsigned
ParseCommandArgUnsigned(const char *s, unsigned max_value);

unsigned
ParseCommandArgUnsigned(const char *s);

bool
ParseCommandArgBool(const char *s);

float
ParseCommandArgFloat(const char *s);

SongTime
ParseCommandArgSongTime(const char *s);

SignedSongTime
ParseCommandArgSignedSongTime(const char *s);

#endif
