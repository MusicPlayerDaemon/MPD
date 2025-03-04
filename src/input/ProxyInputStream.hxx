// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

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
	[[nodiscard]]
	explicit ProxyInputStream(InputStreamPtr _input) noexcept;

	/**
	 * Construct an instance without an #InputStream instance.
	 * Once that instance becomes available, call SetInput().
	 */
	template<typename U>
	[[nodiscard]]
	ProxyInputStream(U &&_uri, Mutex &_mutex) noexcept
		:InputStream(std::forward<U>(_uri), _mutex) {}

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
		    std::span<std::byte> dest) override;

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
