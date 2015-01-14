/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "encoder/EncoderInterface.hxx"
#include "encoder/EncoderPlugin.hxx"
#include "encoder/EncoderList.hxx"
#include "config/ConfigError.hxx"
#include "Log.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/io/FileOutputStream.hxx"
#include "util/Error.hxx"

#include <assert.h>

class RecorderOutput {
	friend struct AudioOutputWrapper<RecorderOutput>;

	AudioOutput base;

	/**
	 * The configured encoder plugin.
	 */
	Encoder *encoder;

	/**
	 * The destination file name.
	 */
	AllocatedPath path;

	/**
	 * The destination file.
	 */
	FileOutputStream *file;

	/**
	 * The buffer for encoder_read().
	 */
	char buffer[32768];

	RecorderOutput()
		:base(recorder_output_plugin),
		 encoder(nullptr),
		 path(AllocatedPath::Null()) {}

	~RecorderOutput() {
		if (encoder != nullptr)
			encoder->Dispose();
	}

	bool Initialize(const config_param &param, Error &error_r) {
		return base.Configure(param, error_r);
	}

	static RecorderOutput *Create(const config_param &param, Error &error);

	bool Configure(const config_param &param, Error &error);

	bool Open(AudioFormat &audio_format, Error &error);
	void Close();

	/**
	 * Writes pending data from the encoder to the output file.
	 */
	bool EncoderToFile(Error &error);

	void SendTag(const Tag &tag);

	size_t Play(const void *chunk, size_t size, Error &error);

private:
	/**
	 * Finish the encoder and commit the file.
	 */
	bool Commit(Error &error);
};

inline bool
RecorderOutput::Configure(const config_param &param, Error &error)
{
	/* read configuration */

	const char *encoder_name =
		param.GetBlockValue("encoder", "vorbis");
	const auto encoder_plugin = encoder_plugin_get(encoder_name);
	if (encoder_plugin == nullptr) {
		error.Format(config_domain,
			     "No such encoder: %s", encoder_name);
		return false;
	}

	path = param.GetBlockPath("path", error);
	if (path.IsNull()) {
		if (!error.IsDefined())
			error.Set(config_domain, "'path' not configured");
		return false;
	}

	/* initialize encoder */

	encoder = encoder_init(*encoder_plugin, param, error);
	if (encoder == nullptr)
		return false;

	return true;
}

RecorderOutput *
RecorderOutput::Create(const config_param &param, Error &error)
{
	RecorderOutput *recorder = new RecorderOutput();

	if (!recorder->Initialize(param, error)) {
		delete recorder;
		return nullptr;
	}

	if (!recorder->Configure(param, error)) {
		delete recorder;
		return nullptr;
	}

	return recorder;
}

inline bool
RecorderOutput::EncoderToFile(Error &error)
{
	assert(file != nullptr);
	assert(file->IsDefined());

	while (true) {
		/* read from the encoder */

		size_t size = encoder_read(encoder, buffer, sizeof(buffer));
		if (size == 0)
			return true;

		/* write everything into the file */

		if (!file->Write(buffer, size, error))
			return false;
	}
}

inline bool
RecorderOutput::Open(AudioFormat &audio_format, Error &error)
{
	/* create the output file */

	file = FileOutputStream::Create(path, error);
	if (file == nullptr)
		return false;

	/* open the encoder */

	if (!encoder->Open(audio_format, error)) {
		delete file;
		return false;
	}

	if (!EncoderToFile(error)) {
		encoder->Close();
		delete file;
		return false;
	}

	return true;
}

inline bool
RecorderOutput::Commit(Error &error)
{
	/* flush the encoder and write the rest to the file */

	bool success = encoder_end(encoder, error) &&
		EncoderToFile(error);

	/* now really close everything */

	encoder->Close();

	if (success && !file->Commit(error))
		success = false;

	delete file;

	return success;
}

inline void
RecorderOutput::Close()
{
	Error error;
	if (!Commit(error))
		LogError(error);
}

inline void
RecorderOutput::SendTag(const Tag &tag)
{
	Error error;
	if (!encoder_pre_tag(encoder, error) ||
	    !EncoderToFile(error) ||
	    !encoder_tag(encoder, tag, error))
		LogError(error);
}

inline size_t
RecorderOutput::Play(const void *chunk, size_t size, Error &error)
{
	return encoder_write(encoder, chunk, size, error) &&
		EncoderToFile(error)
		? size : 0;
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
