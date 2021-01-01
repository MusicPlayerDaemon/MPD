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

#ifndef MPD_PROXY_INPUT_STREAM_HXX
#define MPD_PROXY_INPUT_STREAM_HXX

#include "InputStream.hxx"
#include "Ptr.hxx"
#include "Handler.hxx"
#include "thread/Cond.hxx"

struct Tag;

/**
 * An #InputStream that forwards all methods call to another
 * #InputStream instance.  This can be used as a base class to
 * override selected methods.
 *
 * The inner #InputStream instance may be nullptr initially, to be set
 * later.
 */
class ProxyInputStream : public InputStream, protected InputStreamHandler {
	Cond set_input_cond;

protected:
	InputStreamPtr input;

public:
	explicit ProxyInputStream(InputStreamPtr _input) noexcept;

	/**
	 * Construct an instance without an #InputStream instance.
	 * Once that instance becomes available, call SetInput().
	 */
	ProxyInputStream(const char *_uri,
			 Mutex &_mutex) noexcept
		:InputStream(_uri, _mutex) {}

	~ProxyInputStream() noexcept override;

	ProxyInputStream(const ProxyInputStream &) = delete;
	ProxyInputStream &operator=(const ProxyInputStream &) = delete;

	/* virtual methods from InputStream */
	void Check() override;
	void Update() noexcept override;
	void Seek(std::unique_lock<Mutex> &lock,
		  offset_type new_offset) override;
	bool IsEOF() const noexcept override;
	std::unique_ptr<Tag> ReadTag() noexcept override;
	bool IsAvailable() const noexcept override;
	size_t Read(std::unique_lock<Mutex> &lock,
		    void *ptr, size_t read_size) override;

protected:
	/**
	 * If this instance was initialized without an input, this
	 * method can set it.
	 *
	 * Caller must lock the mutex.
	 */
	void SetInput(InputStreamPtr _input) noexcept;

	/**
	 * Copy public attributes from the underlying input stream to the
	 * "rewind" input stream.  This function is called when a method of
	 * the underlying stream has returned, which may have modified these
	 * attributes.
	 */
	void CopyAttributes();

	/* virtual methods from class InputStreamHandler */
	void OnInputStreamReady() noexcept override {
		InvokeOnReady();
	}

	void OnInputStreamAvailable() noexcept override {
		InvokeOnAvailable();
	}
};

#endif
