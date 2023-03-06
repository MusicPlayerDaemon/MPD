// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ENCODER_INTERFACE_HXX
#define MPD_ENCODER_INTERFACE_HXX

#include <cstddef>
#include <span>

struct AudioFormat;
struct Tag;

class Encoder {
	const bool implements_tag;

public:
	explicit Encoder(bool _implements_tag) noexcept
		:implements_tag(_implements_tag) {}
	virtual ~Encoder() noexcept = default;

	bool ImplementsTag() const noexcept {
		return implements_tag;
	}

	/**
	 * Ends the stream: flushes the encoder object, generate an
	 * end-of-stream marker (if applicable), make everything which
	 * might currently be buffered available by Read().
	 *
	 * After this function has been called, the encoder may not be
	 * usable for more data, and only Read() can be called.
	 *
	 * Throws on error.
	 */
	virtual void End() {
	}

	/**
	 * Flushes an encoder object, make everything which might
	 * currently be buffered available by Read().
	 *
	 * Throws on error.
	 */
	virtual void Flush() {
	}

	/**
	 * Prepare for sending a tag to the encoder.  This is used by
	 * some encoders to flush the previous sub-stream, in
	 * preparation to begin a new one.
	 *
	 * Throws on error.
	 */
	virtual void PreTag() {
	}

	/**
	 * Sends a tag to the encoder.
	 *
	 * Instructions: call PreTag(); then obtain flushed data with
	 * Read(); finally call Tag() and again Read().
	 *
	 * Throws on error.
	 *
	 * @param tag the tag object
	 */
	virtual void SendTag([[maybe_unused]] const Tag &tag) {
	}

	/**
	 * Writes raw PCM data to the encoder.
	 *
	 * Throws on error.
	 *
	 * @param data the buffer containing PCM samples
	 * @param length the length of the buffer in bytes
	 */
	virtual void Write(std::span<const std::byte> src) = 0;

	/**
	 * Reads encoded data from the encoder.
	 *
	 * Call this repeatedly after End(), Flush(), PreTag(), SendTag() and
	 *  Write() until no more data is returned.
	 *
	 * @param buffer a buffer that can be used to write data into
	 *
	 * @return the portion of the buffer that was filled (but may
	 * also point to a different buffer, e.g. one owned by this object)
	 */
	virtual std::span<const std::byte> Read(std::span<std::byte> buffer) noexcept = 0;
};

class PreparedEncoder {
public:
	virtual ~PreparedEncoder() noexcept = default;

	/**
	 * Create an #Encoder instance.
	 *
	 * After this function returns successfully and before the
	 * first Encoder::Write() call, you should invoke
	 * Encoder::Read() to obtain the file header.
	 *
	 * Throws on error.
	 *
	 * @param audio_format the encoder's input audio format; the plugin
	 * may modify the struct to adapt it to its abilities
	 */
	virtual Encoder *Open(AudioFormat &audio_format) = 0;

	/**
	 * Get mime type of encoded content.
	 *
	 * @return an constant string, nullptr on failure
	 */
	virtual const char *GetMimeType() const noexcept {
		return nullptr;
	}
};

#endif
