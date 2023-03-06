// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

struct id3_tag;
class MixRampInfo;

[[gnu::pure]]
MixRampInfo
Id3ToMixRampInfo(const struct id3_tag *tag) noexcept;
