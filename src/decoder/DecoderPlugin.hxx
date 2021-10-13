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

#ifndef MPD_DECODER_PLUGIN_HXX
#define MPD_DECODER_PLUGIN_HXX

#include <forward_list>  // IWYU pragma: export
#include <set>
#include <string>
#include <string_view>

struct ConfigBlock;
class InputStream;
class TagHandler;
class Path;
class DecoderClient;
class DetachedSong;

struct DecoderPlugin {
	const char *name;

	/**
	 * Initialize the decoder plugin.  Optional method.
	 *
	 * @param param a configuration block for this plugin, or nullptr
	 * if none is configured
	 * @return true if the plugin was initialized successfully,
	 * false if the plugin is not available
	 */
	bool (*init)(const ConfigBlock &block) = nullptr;

	/**
	 * Deinitialize a decoder plugin which was initialized
	 * successfully.  Optional method.
	 */
	void (*finish)() noexcept = nullptr;

	/**
	 * Return a set of supported protocols.
	 */
	std::set<std::string> (*protocols)() noexcept = nullptr;

	/**
	 * Decode an URI with a protocol listed in protocols().
	 */
	void (*uri_decode)(DecoderClient &client, const char *uri) = nullptr;

	/**
	 * Decode a stream (data read from an #InputStream object).
	 *
	 * Either implement this method or file_decode().  If
	 * possible, it is recommended to implement this method,
	 * because it is more versatile.
	 */
	void (*stream_decode)(DecoderClient &client, InputStream &is) = nullptr;

	/**
	 * Decode a local file.
	 *
	 * Either implement this method or stream_decode().
	 */
	void (*file_decode)(DecoderClient &client, Path path_fs) = nullptr;

	/**
         * Scan metadata of a file.
         *
	 * Throws on I/O error.
	 *
	 * @return false if the file was not recognized
	 */
	bool (*scan_file)(Path path_fs, TagHandler &handler) = nullptr;

	/**
	 * Scan metadata of a stream.
         *
	 * Throws on I/O error.
	 *
	 * @return false if the stream was not recognized
	 */
	bool (*scan_stream)(InputStream &is, TagHandler &handler) = nullptr;

	/**
	 * @brief Return a "virtual" filename for subtracks in
	 * container formats like flac
	 * @param path_fs full pathname for the file on fs
	 *
	 * @return an empty list if there are no multiple files
	 * a filename for every single track;
	 * do not include full pathname here, just the "virtual" file
	 */
	std::forward_list<DetachedSong> (*container_scan)(Path path_fs) = nullptr;

	/* last element in these arrays must always be a nullptr: */
	const char *const*suffixes = nullptr;
	const char *const*mime_types = nullptr;

	constexpr DecoderPlugin(const char *_name,
				void (*_file_decode)(DecoderClient &client,
						     Path path_fs),
				bool (*_scan_file)(Path path_fs,
						   TagHandler &handler)) noexcept
		:name(_name),
		 file_decode(_file_decode), scan_file(_scan_file) {}

	constexpr DecoderPlugin(const char *_name,
				void (*_stream_decode)(DecoderClient &client,
						       InputStream &is),
				bool (*_scan_stream)(InputStream &is, TagHandler &handler)) noexcept
		:name(_name),
		 stream_decode(_stream_decode),
		 scan_stream(_scan_stream) {}

	constexpr DecoderPlugin(const char *_name,
				void (*_stream_decode)(DecoderClient &client,
						       InputStream &is),
				bool (*_scan_stream)(InputStream &is, TagHandler &handler),
				void (*_file_decode)(DecoderClient &client,
						     Path path_fs),
				bool (*_scan_file)(Path path_fs,
						   TagHandler &handler)) noexcept
		:name(_name),
		 stream_decode(_stream_decode),
		 file_decode(_file_decode),
		 scan_file(_scan_file),
		 scan_stream(_scan_stream) {}

	constexpr auto WithInit(bool (*_init)(const ConfigBlock &block),
				void (*_finish)() noexcept = nullptr) const noexcept {
		auto copy = *this;
		copy.init = _init;
		copy.finish = _finish;
		return copy;
	}

	constexpr auto WithContainer(std::forward_list<DetachedSong> (*_container_scan)(Path path_fs)) const noexcept {
		auto copy = *this;
		copy.container_scan = _container_scan;
		return copy;
	}

	constexpr auto WithProtocols(std::set<std::string> (*_protocols)() noexcept,
				     void (*_uri_decode)(DecoderClient &client, const char *uri)) const noexcept {
		auto copy = *this;
		copy.protocols = _protocols;
		copy.uri_decode = _uri_decode;
		return copy;
	}

	constexpr auto WithSuffixes(const char *const*_suffixes) const noexcept {
		auto copy = *this;
		copy.suffixes = _suffixes;
		return copy;
	}

	constexpr auto WithMimeTypes(const char *const*_mime_types) const noexcept {
		auto copy = *this;
		copy.mime_types = _mime_types;
		return copy;
	}

	/**
	 * Initialize a decoder plugin.
	 *
	 * @param block a configuration block for this plugin
	 * @return true if the plugin was initialized successfully, false if
	 * the plugin is not available
	 */
	bool Init(const ConfigBlock &block) const {
		return init != nullptr
			? init(block)
			: true;
	}

	/**
	 * Deinitialize a decoder plugin which was initialized successfully.
	 */
	void Finish() const {
		if (finish != nullptr)
			finish();
	}

	/**
	 * Decode a stream.
	 */
	void StreamDecode(DecoderClient &client, InputStream &is) const {
		stream_decode(client, is);
	}

	/**
	 * Decode an URI which is supported (check SupportsUri()).
	 */
	void UriDecode(DecoderClient &client, const char *uri) const {
		uri_decode(client, uri);
	}

	/**
	 * Decode a file.
	 */
	template<typename P>
	void FileDecode(DecoderClient &client, P path_fs) const {
		file_decode(client, path_fs);
	}

	/**
	 * Read the tag of a file.
	 */
	template<typename P>
	bool ScanFile(P path_fs, TagHandler &handler) const {
		return scan_file != nullptr
			? scan_file(path_fs, handler)
			: false;
	}

	/**
	 * Read the tag of a stream.
	 */
	bool ScanStream(InputStream &is, TagHandler &handler) const {
		return scan_stream != nullptr
			? scan_stream(is, handler)
			: false;
	}

	/**
	 * return "virtual" tracks in a container
	 */
	template<typename P>
	char *ContainerScan(P path, const unsigned int tnum) const {
		return container_scan(path, tnum);
	}

	[[gnu::pure]]
	bool SupportsUri(const char *uri) const noexcept;

	/**
	 * Does the plugin announce the specified file name suffix?
	 */
	[[gnu::pure]]
	bool SupportsSuffix(std::string_view suffix) const noexcept;

	/**
	 * Does the plugin announce the specified MIME type?
	 */
	[[gnu::pure]]
	bool SupportsMimeType(std::string_view mime_type) const noexcept;

	bool SupportsContainerSuffix(std::string_view suffix) const noexcept {
		return container_scan != nullptr && SupportsSuffix(suffix);
	}
};

#endif
