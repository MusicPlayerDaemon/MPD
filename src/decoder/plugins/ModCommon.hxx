// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_MOD_COMMON_HXX
#define MPD_MOD_COMMON_HXX

#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "util/AllocatedArray.hxx"
#include "util/Domain.hxx"

AllocatedArray<std::byte> mod_loadfile(const Domain *domain, DecoderClient *client, InputStream &is);

#endif
