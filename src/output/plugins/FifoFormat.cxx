/*
 * Copyright 2015-2018 Cary Audio
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

#include "util/StringBuffer.hxx"

struct FifoFormat
{
	const AllocatedPath path;
	std::string path_utf8;

	int input = -1;
	int output = -1;
	bool created = false;

	FifoFormat(const AllocatedPath &_path) : path(_path) {
		if (path.IsNull())
			throw std::runtime_error("No \"format_path\" parameter specified");
		path_utf8 = path.ToUTF8();
		OpenFifo();
	}

	~FifoFormat() {
		CloseFifo();
	}

	void Open(AudioFormat &audio_format);
	void Close();
	void Cancel();

private:
	void Create();
	void Check();
	void Delete();

	void OpenFifo();
	void CloseFifo();

	void Write(const char *str, size_t size);
};

inline void
FifoFormat::Delete()
{
	try {
		RemoveFile(path);
	} catch (...) {
		LogError(std::current_exception(), "Could not remove FIFO");
		return;
	}

	created = false;
}

void
FifoFormat::CloseFifo()
{
	if (input >= 0) {
		close(input);
		input = -1;
	}

	if (output >= 0) {
		close(output);
		output = -1;
	}

	FileInfo fi;
	if (created && GetFileInfo(path, fi))
		Delete();
}

inline void
FifoFormat::Create()
{
	if (!MakeFifo(path, 0666))
		throw FormatErrno("Couldn't create FIFO \"%s\"",
				  path_utf8.c_str());

	created = true;
}

inline void
FifoFormat::Check()
{
	struct stat st;
	if (!StatFile(path, st)) {
		if (errno == ENOENT) {
			/* Path doesn't exist */
			Create();
			return;
		}

		throw FormatErrno("Failed to stat FIFO \"%s\"",
				  path_utf8.c_str());
	}

	if (!S_ISFIFO(st.st_mode))
		throw FormatRuntimeError("\"%s\" already exists, but is not a FIFO",
					 path_utf8.c_str());
}

inline void
FifoFormat::OpenFifo()
try {
	Check();

	input = OpenFile(path, O_RDONLY|O_NONBLOCK|O_BINARY, 0).Steal();
	if (input < 0)
		throw FormatErrno("Could not open FIFO \"%s\" for reading",
				  path_utf8.c_str());

	output = OpenFile(path, O_WRONLY|O_NONBLOCK|O_BINARY, 0).Steal();
	if (output < 0)
		throw FormatErrno("Could not open FIFO \"%s\" for writing",
				  path_utf8.c_str());
} catch (...) {
	CloseFifo();
	throw;
}

void
FifoFormat::Open(AudioFormat &audio_format)
{
	std::string str("open: ");
	str.append(ToString(audio_format).c_str());
	str.append("\n");
	Write(str.c_str(), str.size());
}

void
FifoFormat::Close()
{
	std::string str("close");
	str.append("\n");
	Write(str.c_str(), str.size());
}

void
FifoFormat::Cancel()
{
	//std::string str("cancel");
	//str.append("\n");
	//Write(str.c_str(), str.size());
}

void
FifoFormat::Write(const char *str, size_t size)
{
	ssize_t bytes = write(output, str, size);
	if (bytes < 0 &&
		(errno != EAGAIN && errno != EINTR)) {
		throw FormatErrno("Failed to write to FIFO %s", path_utf8.c_str());
	}
}
