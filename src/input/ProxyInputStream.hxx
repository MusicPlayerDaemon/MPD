/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_PROXY_INPUT_STREAM_HXX
#define MPD_PROXY_INPUT_STREAM_HXX

#include "InputStream.hxx"

struct Tag;

/**
 * An #InputStream that forwards all methods call to another
 * #InputStream instance.  This can be used as a base class to
 * override selected methods.
 */
class ProxyInputStream : public InputStream {
protected:
	InputStream &input;

public:
	gcc_nonnull_all
	ProxyInputStream(InputStream *_input);

	virtual ~ProxyInputStream();

	ProxyInputStream(const ProxyInputStream &) = delete;
	ProxyInputStream &operator=(const ProxyInputStream &) = delete;

	/* virtual methods from InputStream */
	bool Check(Error &error) override;
	void Update() override;
	bool Seek(offset_type new_offset, Error &error) override;
	bool IsEOF() override;
	Tag *ReadTag() override;
	bool IsAvailable() override;
	size_t Read(void *ptr, size_t read_size, Error &error) override;

protected:
	/**
	 * Copy public attributes from the underlying input stream to the
	 * "rewind" input stream.  This function is called when a method of
	 * the underlying stream has returned, which may have modified these
	 * attributes.
	 */
	void CopyAttributes();
};

#endif
