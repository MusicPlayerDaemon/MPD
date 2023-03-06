// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_VORBIS_COMMENTS_HXX
#define MPD_VORBIS_COMMENTS_HXX

#include <memory>

struct vorbis_comment;
struct ReplayGainInfo;
class TagHandler;
struct Tag;

bool
VorbisCommentToReplayGain(ReplayGainInfo &rgi,
			  const vorbis_comment &vc) noexcept;

void
VorbisCommentScan(const vorbis_comment &vc, TagHandler &handler) noexcept;

std::unique_ptr<Tag>
VorbisCommentToTag(const vorbis_comment &vc) noexcept;

#endif
