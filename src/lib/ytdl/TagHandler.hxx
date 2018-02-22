#ifndef MPD_LIB_YTDL_TAG_HANDLER_HXX
#define MPD_LIB_YTDL_TAG_HANDLER_HXX

#include "Handler.hxx"

#include "util/StringView.hxx"
#include "tag/Builder.hxx"
#include <forward_list>
#include <string>
#include <map>

namespace Ytdl {

class TagHandler: public MetadataHandler {
	template<typename T>
	struct Prioritise {
		int priority = -1;
		T value;

		void ReplaceWith(int new_priority, const T &new_value) noexcept {
			if (new_priority > priority) {
				value = new_value;
				priority = new_priority;
			}
		}
	};

	std::unique_ptr<TagBuilder> builder;
	std::forward_list<TagHandler> entries;
	std::multimap<std::string, std::string> headers;
	std::string extractor;
	std::string url;
	std::string webpage_url;
	std::string type;
	Prioritise<std::string> artist;
	int playlist_index = -1;

	TagHandler *current_entry = nullptr;

	void SortEntries();

	// MetadataHandler virtual callbacks
	ParseContinue OnEntryStart() noexcept override;
	ParseContinue OnEntryEnd() noexcept override;
	ParseContinue OnEnd() noexcept override;
	ParseContinue OnMetadata(StringMetadataTag tag, StringView value) noexcept override;
	ParseContinue OnMetadata(IntMetadataTag tag, long long int value) noexcept override;
	ParseContinue OnHeader(StringView header, StringView value) noexcept override;

public:
	TagHandler();

	const std::string &GetURL() const noexcept {
		return url;
	}

	const std::multimap<std::string, std::string> &GetHeaders() const noexcept {
		return headers;
	}

	const std::string &GetWebpageURL() const noexcept {
		return webpage_url;
	}

	const std::string &GetExtractor() const noexcept {
		return extractor;
	}

	TagBuilder& GetTagBuilder() {
		return *builder;
	}

	std::forward_list<TagHandler> &GetEntries() noexcept {
		return entries;
	}
};

} // namespace ytdl

#endif
