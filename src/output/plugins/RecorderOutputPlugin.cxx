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

#include "RecorderOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "tag/Format.hxx"
#include "encoder/ToOutputStream.hxx"
#include "encoder/EncoderInterface.hxx"
#include "encoder/Configured.hxx"
#include "config/Path.hxx"
#include "Log.hxx"
#include "fs/AllocatedPath.hxx"
#include "io/FileOutputStream.hxx"
#include "util/Domain.hxx"
#include "util/ScopeExit.hxx"

#include <cassert>
#include <memory>
#include <stdexcept>

#include <stdlib.h>

static constexpr Domain recorder_domain("recorder");

class RecorderOutput final : AudioOutput {
	/**
	 * The configured encoder plugin.
	 */
	std::unique_ptr<PreparedEncoder> prepared_encoder;
	Encoder *encoder;

	/**
	 * The destination file name.
	 */
	AllocatedPath path = nullptr;

	/**
	 * A string that will be used with FormatTag() to build the
	 * destination path.
	 */
	std::string format_path;

	/**
	 * The #AudioFormat that is currently active.  This is used
	 * for switching to another file.
	 */
	AudioFormat effective_audio_format;

	/**
	 * The destination file.
	 */
	FileOutputStream *file;

	explicit RecorderOutput(const ConfigBlock &block);

public:
	static AudioOutput *Create(EventLoop &, const ConfigBlock &block) {
		return new RecorderOutput(block);
	}

private:
	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	/**
	 * Writes pending data from the encoder to the output file.
	 */
	void EncoderToFile();

	void SendTag(const Tag &tag) override;

	size_t Play(const void *chunk, size_t size) override;

	[[nodiscard]] gcc_pure
	bool HasDynamicPath() const noexcept {
		return !format_path.empty();
	}

	/**
	 * Finish the encoder and commit the file.
	 *
	 * Throws on error.
	 */
	void Commit();

	void FinishFormat();
	void ReopenFormat(AllocatedPath &&new_path);
};

RecorderOutput::RecorderOutput(const ConfigBlock &block)
	:AudioOutput(0),
	 prepared_encoder(CreateConfiguredEncoder(block))
{
	/* read configuration */

	path = block.GetPath("path");

	const char *fmt = block.GetBlockValue("format_path", nullptr);
	if (fmt != nullptr)
		format_path = fmt;

	if (path.IsNull() && fmt == nullptr)
		throw std::runtime_error("'path' not configured");

	if (!path.IsNull() && fmt != nullptr)
		throw std::runtime_error("Cannot have both 'path' and 'format_path'");
}

inline void
RecorderOutput::EncoderToFile()
{
	assert(file != nullptr);

	EncoderToOutputStream(*file, *encoder);
}

void
RecorderOutput::Open(AudioFormat &audio_format)
{
	/* create the output file */

	if (!HasDynamicPath()) {
		assert(!path.IsNull());

		file = new FileOutputStream(path);
	} else {
		/* don't open the file just yet; wait until we have
		   a tag that we can use to build the path */
		assert(path.IsNull());

		file = nullptr;
	}

	/* open the encoder */

	try {
		encoder = prepared_encoder->Open(audio_format);
	} catch (...) {
		delete file;
		throw;
	}

	if (!HasDynamicPath()) {
		try {
			EncoderToFile();
		} catch (...) {
			delete encoder;
			throw;
		}
	} else {
		/* remember the AudioFormat for ReopenFormat() */
		effective_audio_format = audio_format;

		/* close the encoder for now; it will be opened as
		   soon as we have received a tag */
		delete encoder;
	}
}

inline void
RecorderOutput::Commit()
{
	assert(!path.IsNull());

	/* flush the encoder and write the rest to the file */

	try {
		encoder->End();
		EncoderToFile();
	} catch (...) {
		delete encoder;
		throw;
	}

	/* now really close everything */

	delete encoder;

	try {
		file->Commit();
	} catch (...) {
		delete file;
		throw;
	}

	delete file;
}

void
RecorderOutput::Close() noexcept
{
	if (file == nullptr) {
		/* not currently encoding to a file; nothing needs to
		   be done now */
		assert(HasDynamicPath());
		assert(path.IsNull());
		return;
	}

	try {
		Commit();
	} catch (...) {
		LogError(std::current_exception());
	}

	if (HasDynamicPath()) {
		assert(!path.IsNull());
		path.SetNull();
	}
}

void
RecorderOutput::FinishFormat()
{
	assert(HasDynamicPath());

	if (file == nullptr)
		return;

	try {
		Commit();
	} catch (...) {
		LogError(std::current_exception());
	}

	file = nullptr;
	path.SetNull();
}

inline void
RecorderOutput::ReopenFormat(AllocatedPath &&new_path)
{
	assert(HasDynamicPath());
	assert(path.IsNull());
	assert(file == nullptr);

	auto *new_file = new FileOutputStream(new_path);

	AudioFormat new_audio_format = effective_audio_format;

	try {
		encoder = prepared_encoder->Open(new_audio_format);
	} catch (...) {
		delete new_file;
		throw;
	}

	/* reopening the encoder must always result in the same
	   AudioFormat as before */
	assert(new_audio_format == effective_audio_format);

	try {
		EncoderToOutputStream(*new_file, *encoder);
	} catch (...) {
		delete encoder;
		delete new_file;
		throw;
	}

	path = std::move(new_path);
	file = new_file;

	FmtDebug(recorder_domain, "Recording to \"{}\"", path);
}

void
RecorderOutput::SendTag(const Tag &tag)
{
	if (HasDynamicPath()) {
		char *p = FormatTag(tag, format_path.c_str());
		if (p == nullptr || *p == 0) {
			/* no path could be composed with this tag:
			   don't write a file */
			free(p);
			FinishFormat();
			return;
		}

		AtScopeExit(p) { free(p); };

		AllocatedPath new_path = nullptr;

		try {
			new_path = ParsePath(p);
		} catch (...) {
			LogError(std::current_exception());
			FinishFormat();
			return;
		}

		if (new_path != path) {
			FinishFormat();

			try {
				ReopenFormat(std::move(new_path));
			} catch (...) {
				LogError(std::current_exception());
				return;
			}
		}
	}

	encoder->PreTag();
	EncoderToFile();
	encoder->SendTag(tag);
}

size_t
RecorderOutput::Play(const void *chunk, size_t size)
{
	if (file == nullptr) {
		/* not currently encoding to a file; discard incoming
		   data */
		assert(HasDynamicPath());
		assert(path.IsNull());
		return size;
	}

	encoder->Write(chunk, size);

	EncoderToFile();

	return size;
}

const struct AudioOutputPlugin recorder_output_plugin = {
	"recorder",
	nullptr,
	&RecorderOutput::Create,
	nullptr,
};
