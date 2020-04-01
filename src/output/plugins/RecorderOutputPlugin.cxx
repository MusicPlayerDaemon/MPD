/*
 * Copyright 2003-2020 The Music Player Daemon Project
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
#include "tag/Format.hxx"
#include "encoder/ToOutputStream.hxx"
#include "encoder/EncoderInterface.hxx"
#include "encoder/Configured.hxx"
#include "config/Path.hxx"
#include "Log.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/io/FileOutputStream.hxx"
#include "util/Domain.hxx"
#include "util/ScopeExit.hxx"
#include "thread/Thread.hxx"
#include "thread/Name.hxx"
#include "output/Filtered.hxx"
#include <cassert>
#include <memory>
#include <stdexcept>
#include <fstream>

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
	 * The archive destination file name.
	 */
	AllocatedPath archive_path = nullptr;

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

private:
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
	
	void ArchiveTask();

	/* parent (true) recorder to send archive message to */
	int archive_requested = 0;
	int can_archive = 0;
	int delete_after_record = 0;
	std::string archive_format_path;
	Thread archive_thread;
	char *archive_source = NULL;
	char *archive_dest = NULL;
};

RecorderOutput::RecorderOutput(const ConfigBlock &block)
	:AudioOutput(0),
	 prepared_encoder(CreateConfiguredEncoder(block)),
	 archive_thread(BIND_THIS_METHOD(ArchiveTask))
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

	const char *archive_fmt = block.GetBlockValue("archive_path", nullptr);
	if (archive_fmt != nullptr) {
		archive_format_path = archive_fmt;
	}

	if (block.GetBlockValue("delete_after_record", nullptr))
		delete_after_record = 1;
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

	/* move file to archive if requested */
	if (archive_requested && can_archive) {

		// wait for previous move operation to finish
		if (archive_thread.IsDefined())
			archive_thread.Join();

		// run move/copy asynchronously to not block output operation
		//
		archive_source = strdup(path.c_str());
		archive_dest = strdup(archive_path.c_str());

		archive_thread.Start();

		archive_requested = 0;
	}
	else
	{
	    /* delete file if requested */
	    if (delete_after_record) {
		    FormatDebug(recorder_domain, "Deleting \"%s\"", path.c_str());
		    
		    unlink(path.c_str());
	    }
	}

	delete file;
}

void
RecorderOutput::ArchiveTask() {
	int rc;

	SetThreadName("archive_file");

	if (!archive_source || !archive_dest)
		return;

	if (delete_after_record) {
		rc = rename (archive_source, archive_dest);
	} else {
		try {
			std::ifstream  src(archive_source, std::ios::binary);
			std::ofstream  dst(archive_dest,   std::ios::binary);

			dst << src.rdbuf();

			rc = 0;
		} 
		catch(...)
		{
			rc = -1;
		}
	}

	if (rc == 0) {
		FormatDebug(recorder_domain, "%s \"%s\" to \"%s\"",
			delete_after_record ? "Moved" : "Copied",
			archive_source, archive_dest);
	} else {
		FormatError(recorder_domain, "Failed to %s \"%s\" to \"%s\", rc=%d",
			delete_after_record ? "move" : "copy",
			archive_source, archive_dest, errno);
	}


	free (archive_source);
	free (archive_dest);

	archive_source = archive_dest = NULL;
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

	FormatDebug(recorder_domain, "Recording to \"%s\"",
		    path.ToUTF8().c_str());
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

		can_archive = 0;

		if (!archive_format_path.empty()) {
			char *ap = FormatTag(tag, archive_format_path.c_str());
			AtScopeExit(ap) { free(ap); };

			try {
				archive_path = ParsePath(ap);
				can_archive = 1;
			} catch (const std::runtime_error &e) {
				LogError(e);
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
