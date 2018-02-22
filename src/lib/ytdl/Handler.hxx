#ifndef MPD_LIB_YTDL_HANDLER_HXX
#define MPD_LIB_YTDL_HANDLER_HXX

#include "util/StringView.hxx"

namespace Ytdl {

enum class StringMetadataTag {
	CREATOR,
	DESCRIPTION,
	EXTRACTOR,
	PLAYLIST_TITLE,
	TITLE,
	TYPE,
	UPLOAD_DATE,
	UPLOADER_NAME,
	UPLOADER_ID,
	URL,
	WEBPAGE_URL,
};

enum class IntMetadataTag {
	DURATION_MS,
	PLAYLIST_INDEX,
};

enum class ParseContinue {
	CONTINUE,
	CANCEL,
};

struct MetadataHandler {
	virtual ParseContinue OnEntryStart() noexcept = 0;
	virtual ParseContinue OnEntryEnd() noexcept = 0;
	virtual ParseContinue OnEnd() noexcept = 0;
	virtual ParseContinue OnMetadata(StringMetadataTag tag, StringView value) noexcept = 0;
	virtual ParseContinue OnMetadata(IntMetadataTag tag, long long int value) noexcept = 0;
	virtual ParseContinue OnHeader(StringView header, StringView value) noexcept = 0;
};

} // namespace Ytdl

#endif
