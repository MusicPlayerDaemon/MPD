// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "tag/Builder.hxx"
#include "tag/Tag.hxx"

inline void
BuildTag([[maybe_unused]] TagBuilder &tag) noexcept
{
}

template<typename... Args>
inline void
BuildTag(TagBuilder &tag, TagType type, const char *value,
	 Args&&... args) noexcept
{
	tag.AddItem(type, value);
	BuildTag(tag, std::forward<Args>(args)...);
}

template<typename... Args>
inline Tag
MakeTag(Args&&... args) noexcept
{
	TagBuilder tag;
	BuildTag(tag, std::forward<Args>(args)...);
	return tag.Commit();
}
