/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_TAG_HANDLER_HXX
#define MPD_TAG_HANDLER_HXX

#include "Type.h"
#include "Chrono.hxx"
#include "util/Compiler.h"

template<typename T> struct ConstBuffer;
struct StringView;
struct AudioFormat;
class TagBuilder;

/**
 * An interface for receiving metadata of a song.
 */
class TagHandler {
	const unsigned want_mask;

public:
	static constexpr unsigned WANT_DURATION = 0x1;
	static constexpr unsigned WANT_TAG = 0x2;
	static constexpr unsigned WANT_PAIR = 0x4;
	static constexpr unsigned WANT_AUDIO_FORMAT = 0x8;
	static constexpr unsigned WANT_PICTURE = 0x10;

	explicit TagHandler(unsigned _want_mask) noexcept
		:want_mask(_want_mask) {}

	TagHandler(const TagHandler &) = delete;
	TagHandler &operator=(const TagHandler &) = delete;

	bool WantDuration() const noexcept {
		return want_mask & WANT_DURATION;
	}

	bool WantTag() const noexcept {
		return want_mask & WANT_TAG;
	}

	bool WantPair() const noexcept {
		return want_mask & WANT_PAIR;
	}

	bool WantAudioFormat() const noexcept {
		return want_mask & WANT_AUDIO_FORMAT;
	}

	bool WantPicture() const noexcept {
		return want_mask & WANT_PICTURE;
	}

	/**
	 * Declare the duration of a song.  Do not call
	 * this when the duration could not be determined, because
	 * there is no magic value for "unknown duration".
	 */
	virtual void OnDuration(SongTime duration) noexcept = 0;

	/**
	 * A tag has been read.
	 *
	 * @param the value of the tag; the pointer will become
	 * invalid after returning
	 */
	virtual void OnTag(TagType type, StringView value) noexcept = 0;

	/**
	 * A name-value pair has been read.  It is the codec specific
	 * representation of tags.
	 */
	virtual void OnPair(StringView key, StringView value) noexcept = 0;

	/**
	 * Declare the audio format of a song.
	 *
	 * Because the #AudioFormat type is limited to formats
	 * supported by MPD, the value passed to this method may be an
	 * approximation (should be the one passed to
	 * DecoderClient::Ready()).  For example, some codecs such as
	 * MP3 are bit depth agnostic, so the decoder plugin chooses a
	 * bit depth depending on what the codec library emits.
	 *
	 * This method is only called by those decoder plugins which
	 * implement it.  Some may not have any code for calling it,
	 * and others may decide that determining the audio format is
	 * too expensive.
	 */
	virtual void OnAudioFormat(AudioFormat af) noexcept = 0;

	/**
	 * A picture has been read.
	 *
	 * This method will only be called if #WANT_PICTURE was enabled.
	 *
	 * @param mime_type an optional MIME type string
	 * @param buffer the picture file contents; the buffer will be
	 * invalidated after this method returns
	 */
	virtual void OnPicture(const char *mime_type,
			       ConstBuffer<void> buffer) noexcept = 0;
};

class NullTagHandler : public TagHandler {
public:
	explicit NullTagHandler(unsigned _want_mask) noexcept
		:TagHandler(_want_mask) {}

	void OnDuration([[maybe_unused]] SongTime duration) noexcept override {}
	void OnTag(TagType type, StringView value) noexcept override;
	void OnPair(StringView key, StringView value) noexcept override;
	void OnAudioFormat(AudioFormat af) noexcept override;
	void OnPicture(const char *mime_type,
		       ConstBuffer<void> buffer) noexcept override;
};

/**
 * This #TagHandler implementation adds tag values to a #TagBuilder
 * object.
 */
class AddTagHandler : public NullTagHandler {
protected:
	TagBuilder &tag;

	AddTagHandler(unsigned _want_mask, TagBuilder &_builder) noexcept
		:NullTagHandler(WANT_DURATION|WANT_TAG|_want_mask),
		 tag(_builder) {}

public:
	explicit AddTagHandler(TagBuilder &_builder) noexcept
		:AddTagHandler(0, _builder) {}

	void OnDuration(SongTime duration) noexcept override;
	void OnTag(TagType type, StringView value) noexcept override;
};

/**
 * This #TagHandler implementation adds tag values to a #TagBuilder object
 * (casted from the context pointer), and supports the has_playlist
 * attribute.
 */
class FullTagHandler : public AddTagHandler {
	AudioFormat *const audio_format;

protected:
	FullTagHandler(unsigned _want_mask, TagBuilder &_builder,
		       AudioFormat *_audio_format) noexcept
		:AddTagHandler(WANT_PAIR|_want_mask
			       |(_audio_format ? WANT_AUDIO_FORMAT : 0),
			       _builder),
		 audio_format(_audio_format) {}

public:
	explicit FullTagHandler(TagBuilder &_builder,
				AudioFormat *_audio_format=nullptr) noexcept
		:FullTagHandler(0, _builder, _audio_format) {}

	void OnPair(StringView key, StringView value) noexcept override;
	void OnAudioFormat(AudioFormat af) noexcept override;
};

#endif
