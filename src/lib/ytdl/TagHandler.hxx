#ifndef MPD_LIB_YTDL_TAG_HANDLER_HXX
#define MPD_LIB_YTDL_TAG_HANDLER_HXX

#include "Handler.hxx"

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

	Prioritise<std::string> artist;

	TagHandler *current_entry = nullptr;

public:
	std::unique_ptr<TagBuilder> builder;
	std::forward_list<TagHandler> entries;
	std::multimap<std::string, std::string> headers;
	std::string extractor;
	std::string url;
	std::string webpage_url;
	std::string type;
	int playlist_index = -1;

	TagHandler();
	void SortEntries();

	ParseContinue OnEntryStart() noexcept;
	ParseContinue OnEntryEnd() noexcept;
	ParseContinue OnEnd() noexcept;
	ParseContinue OnMetadata(StringMetadataTag tag, StringView value) noexcept;
	ParseContinue OnMetadata(IntMetadataTag tag, long long int value) noexcept;
	ParseContinue OnHeader(StringView header, StringView value) noexcept;
};

} // namespace ytdl

#endif
