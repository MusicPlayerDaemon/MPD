#include "TagHandler.hxx"

namespace Ytdl {

enum class PriorityArtist {
	UPLOADER_ID,
	UPLOADER_NAME,
	CREATOR,
};

TagHandler::TagHandler() : builder(std::make_unique<TagBuilder>()) { }

void
TagHandler::SortEntries()
{
	struct TagHandlerSort {
		inline bool
		operator() (const TagHandler& tag0, const TagHandler& tag1)
		{
			return tag0.playlist_index < tag1.playlist_index;
		}
	};

	entries.sort<TagHandlerSort>(TagHandlerSort());
}

ParseContinue
TagHandler::OnEntryStart() noexcept
{
	try {
		entries.emplace_front();
		current_entry = &entries.front();

		return ParseContinue::CONTINUE;
	} catch (std::bad_alloc&) {
		return ParseContinue::CANCEL;
	}
}

ParseContinue
TagHandler::OnEntryEnd() noexcept
{
	if (current_entry) {
		current_entry->OnEnd();
		current_entry = nullptr;
		return ParseContinue::CONTINUE;
	} else {
		// bad parser state
		return ParseContinue::CANCEL;
	}
}

ParseContinue
TagHandler::OnEnd() noexcept
{
	if (!artist.value.empty()) {
		builder->AddItem(TAG_ARTIST, artist.value);
	}

	this->SortEntries();

	return ParseContinue::CONTINUE;
}

ParseContinue
TagHandler::OnMetadata(StringMetadataTag tag, StringView value) noexcept
{
	if (current_entry) {
		return current_entry->OnMetadata(tag, value);
	}

	try {
		switch (tag) {
			case StringMetadataTag::CREATOR:
				artist.ReplaceWith((int)PriorityArtist::CREATOR, value.ToString());
				break;
			case StringMetadataTag::DESCRIPTION:
				builder->AddItem(TAG_COMMENT, value);
				break;
			case StringMetadataTag::TITLE:
				builder->AddItem(TAG_TITLE, value);
				break;
			case StringMetadataTag::UPLOAD_DATE:
				builder->AddItem(TAG_DATE, value);
				break;
			case StringMetadataTag::UPLOADER_NAME:
				artist.ReplaceWith((int)PriorityArtist::UPLOADER_NAME, value.ToString());
				break;
			case StringMetadataTag::UPLOADER_ID:
				artist.ReplaceWith((int)PriorityArtist::UPLOADER_ID, value.ToString());
				break;
			case StringMetadataTag::URL:
				url = value.ToString();
				break;
			case StringMetadataTag::WEBPAGE_URL:
				webpage_url = value.ToString();
				break;
			case StringMetadataTag::TYPE:
				type = value.ToString();
				break;
			case StringMetadataTag::EXTRACTOR:
				extractor = value.ToString();
				break;
			default:
				break;
		}

		return ParseContinue::CONTINUE;
	} catch (std::bad_alloc&) {
		return ParseContinue::CANCEL;
	}
}

ParseContinue
TagHandler::OnMetadata(IntMetadataTag tag, long long int value) noexcept
{
	if (current_entry) {
		return current_entry->OnMetadata(tag, value);
	}

	switch (tag) {
		case IntMetadataTag::DURATION_MS:
			builder->SetDuration(SignedSongTime::FromMS(value));
			break;
		case IntMetadataTag::PLAYLIST_INDEX:
			playlist_index = value;
			break;
		default:
			break;
	}

	return ParseContinue::CONTINUE;
}

ParseContinue
TagHandler::OnHeader(StringView header, StringView value) noexcept
{
	if (current_entry) {
		return current_entry->OnHeader(header, value);
	}

	try {
		headers.emplace(std::string(header.data, header.size),
			std::string(value.data, value.size));

		return ParseContinue::CONTINUE;
	} catch (std::bad_alloc&) {
		return ParseContinue::CANCEL;
	}
}

} // namespace Ytdl
