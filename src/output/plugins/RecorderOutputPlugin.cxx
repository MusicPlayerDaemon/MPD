/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "config.h"
#include "RecorderOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "../Wrapper.hxx"
#include "tag/Format.hxx"
#include "encoder/ToOutputStream.hxx"
#include "encoder/EncoderInterface.hxx"
#include "encoder/EncoderPlugin.hxx"
#include "encoder/EncoderList.hxx"
#include "config/ConfigError.hxx"
#include "config/ConfigPath.hxx"
#include "Log.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/io/FileOutputStream.hxx"
#include "util/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "util/ScopeExit.hxx"

#include <stdexcept>

#include <assert.h>
#include <stdlib.h>

static constexpr Domain recorder_domain("recorder");

class RecorderOutput {
	friend struct AudioOutputWrapper<RecorderOutput>;

	AudioOutput base;

	/**
	 * The configured encoder plugin.
	 */
	PreparedEncoder *prepared_encoder = nullptr;
	Encoder *encoder;

	/**
	 * The destination file name.
	 */
	AllocatedPath path = AllocatedPath::Null();

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

	RecorderOutput(const ConfigBlock &block);

	~RecorderOutput() {
		delete prepared_encoder;
	}

	static RecorderOutput *Create(const ConfigBlock &block);

	void Open(AudioFormat &audio_format);
	void Close();

	/**
	 * Writes pending data from the encoder to the output file.
	 */
	void EncoderToFile();

	void SendTag(const Tag &tag);

	size_t Play(const void *chunk, size_t size);

private:
	gcc_pure
	bool HasDynamicPath() const {
		return !format_path.empty();
	}

	/**
	 * Finish the encoder and commit the file.
	 *
	 * Throws #std::runtime_error on error.
	 */
	void Commit();

	void FinishFormat();
	void ReopenFormat(AllocatedPath &&new_path);
};

RecorderOutput::RecorderOutput(const ConfigBlock &block)
	:base(recorder_output_plugin, block)
{
	/* read configuration */

	const char *encoder_name =
		block.GetBlockValue("encoder", "vorbis");
	const auto encoder_plugin = encoder_plugin_get(encoder_name);
	if (encoder_plugin == nullptr)
		throw FormatRuntimeError("No such encoder: %s", encoder_name);

	path = block.GetPath("path");

	const char *fmt = block.GetBlockValue("format_path", nullptr);
	if (fmt != nullptr)
		format_path = fmt;

	if (path.IsNull() && fmt == nullptr)
		throw std::runtime_error("'path' not configured");

	if (!path.IsNull() && fmt != nullptr)
		throw std::runtime_error("Cannot have both 'path' and 'format_path'");

	/* initialize encoder */

	prepared_encoder = encoder_init(*encoder_plugin, block);
}

RecorderOutput *
RecorderOutput::Create(const ConfigBlock &block)
{
	return new RecorderOutput(block);
}

inline void
RecorderOutput::EncoderToFile()
{
	assert(file != nullptr);

	EncoderToOutputStream(*file, *encoder);
}

inline void
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
	} catch (const std::runtime_error &) {
		delete file;
		throw;
	}

	if (!HasDynamicPath()) {
		try {
			EncoderToFile();
		} catch (const std::runtime_error &) {
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

inline void
RecorderOutput::Close()
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
	} catch (const std::exception &e) {
		LogError(e);
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
	} catch (const std::exception &e) {
		LogError(e);
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

	FileOutputStream *new_file = new FileOutputStream(new_path);

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
	} catch (const std::exception &e) {
		delete encoder;
		delete new_file;
		throw;
	}

	path = std::move(new_path);
	file = new_file;

	FormatDebug(recorder_domain, "Recording to \"%s\"",
		    path.ToUTF8().c_str());
}

inline void
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

		AllocatedPath new_path = AllocatedPath::Null();

		try {
			new_path = ParsePath(p);
		} catch (const std::runtime_error &e) {
			LogError(e);
			FinishFormat();
			return;
		}

		if (new_path != path) {
			FinishFormat();

			try {
				ReopenFormat(std::move(new_path));
			} catch (const std::runtime_error &e) {
				LogError(e);
				return;
			}
		}
	}

	encoder->PreTag();
	EncoderToFile();
	encoder->SendTag(tag);
}

inline size_t
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

typedef AudioOutputWrapper<RecorderOutput> Wrapper;

const struct AudioOutputPlugin recorder_output_plugin = {
	"recorder",
	nullptr,
	&Wrapper::Init,
	&Wrapper::Finish,
	nullptr,
	nullptr,
	&Wrapper::Open,
	&Wrapper::Close,
	nullptr,
	&Wrapper::SendTag,
	&Wrapper::Play,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};
