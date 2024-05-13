// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ICY_INPUT_STREAM_HXX
#define MPD_ICY_INPUT_STREAM_HXX

#include "ProxyInputStream.hxx"

#include <memory>

struct Tag;
class IcyMetaDataParser;

/**
 * An #InputStream filter that parses Icy metadata.
 */
class IcyInputStream final : public ProxyInputStream {
	std::shared_ptr<IcyMetaDataParser> parser;

	/**
	 * The #Tag object ready to be requested via ReadTag().
	 */
	std::unique_ptr<Tag> input_tag;

	/**
	 * The #Tag object ready to be requested via ReadTag().
	 */
	std::unique_ptr<Tag> icy_tag;

	offset_type override_offset = 0;

public:
	/**
	 * @param _parser a IcyMetaDataParser instance which is shared
	 * with our input; it needs to be shared because our input
	 * needs to feed parameters (e.g. from the "icy-metaint"
	 * header) into it
	 *
	 * Throws on error (e.g. if the charset converter specified by
	 * the URI fragment fails to initialize).
	 */
	IcyInputStream(InputStreamPtr _input,
		       std::shared_ptr<IcyMetaDataParser> _parser);
	~IcyInputStream() noexcept override;

	IcyInputStream(const IcyInputStream &) = delete;
	IcyInputStream &operator=(const IcyInputStream &) = delete;

	[[gnu::pure]]
	bool IsEnabled() const noexcept;

	/* virtual methods from InputStream */
	void Update() noexcept override;
	std::unique_ptr<Tag> ReadTag() noexcept override;
	size_t Read(std::unique_lock<Mutex> &lock,
		    std::span<std::byte> dest) override;
};

#endif
