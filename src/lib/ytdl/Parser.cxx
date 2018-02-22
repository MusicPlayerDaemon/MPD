#include "config.h"
#include "lib/yajl/Callbacks.hxx"
#include "Parser.hxx"
#include "Handler.hxx"

namespace Ytdl {

enum class State {
	NONE,
	TITLE,
	DURATION,
	UPLOAD_DATE,
	UPLOADER,
	UPLOADER_ID,
	CREATOR,
	DESCRIPTION,
	WEBPAGE_URL,
	PLAYLIST_TITLE,
	PLAYLIST_INDEX,
	EXTRACTOR,
	TYPE,
	HEADERS,
	ENTRIES,
	URL,
};

class ParserContext {
	MetadataHandler &handler;
	size_t depth = 0;
	State state = State::NONE;
	State entry_state = State::NONE;
	std::string header_key;

	struct ParserData {
		State &state;
		size_t depth;

		ParserData(State &_state, size_t _depth): state(_state), depth(_depth) { }
	};

	ParserData CurrentState() noexcept;

	int Result(ParseContinue cont) {
		switch (cont) {
			case ParseContinue::CONTINUE:
				return 1;
			case ParseContinue::CANCEL:
				return 0;
		}
	}

public:
	int Integer(long long integerVal) noexcept;
	int Double(double doubleVal) noexcept;
	int String(StringView value) noexcept;
	int StartMap() noexcept;
	int MapKey(StringView key) noexcept;
	int EndMap() noexcept;
	int StartArray() noexcept;
	int EndArray() noexcept;

	ParserContext(MetadataHandler &_handler): handler(_handler) { }
};

ParserContext::ParserData
ParserContext::CurrentState() noexcept
{
	if (state == State::ENTRIES && depth >= 2) {
		return ParserData(entry_state, depth - 2);
	} else {
		return ParserData(state, depth);
	}
}

int
ParserContext::StartArray() noexcept
{
	depth++;

	ParserData data = CurrentState();
	data.state = State::NONE;

	return 1;
}

int
ParserContext::EndArray() noexcept
{
	depth--;

	ParserData data = CurrentState();
	data.state = State::NONE;

	return 1;
}

int
ParserContext::StartMap() noexcept
{
	depth++;

	ParserData data = CurrentState();
	if (state == State::ENTRIES && data.depth == 1) {
		data.state = State::NONE;
		return Result(handler.OnEntryStart());
	} else if (data.state != State::HEADERS || data.depth != 2) {
		data.state = State::NONE;
	}

	return 1;
}

int
ParserContext::EndMap() noexcept
{
	depth--;

	ParserData data = CurrentState();

	if (state == State::ENTRIES && data.depth == 0) {
		data.state = State::NONE;
		return Result(handler.OnEntryEnd());
	} else if (depth == 0) {
		return Result(handler.OnEnd());
	} else {
		data.state = State::NONE;
	}

	return 1;
}

int
ParserContext::MapKey(StringView key) noexcept
{
	ParserData data = CurrentState();

	if (data.depth == 1) {
		if (key.Equals("title")) {
			data.state = State::TITLE;
		} else if (key.Equals("duration")) {
			data.state = State::DURATION;
		} else if (key.Equals("upload_date")) {
			data.state = State::UPLOAD_DATE;
		} else if (key.Equals("uploader")) {
			data.state = State::UPLOADER;
		} else if (key.Equals("uploader_id")) {
			data.state = State::UPLOADER_ID;
		} else if (key.Equals("creator")) {
			data.state = State::CREATOR;
		} else if (key.Equals("description")) {
			data.state = State::DESCRIPTION;
		} else if (key.Equals("webpage_url")) {
			data.state = State::WEBPAGE_URL;
		} else if (key.Equals("playlist_title")) {
			data.state = State::PLAYLIST_TITLE;
		} else if (key.Equals("playlist_index")) {
			data.state = State::PLAYLIST_INDEX;
		} else if (key.Equals("extractor_key") || key.Equals("ie_key")) {
			data.state = State::EXTRACTOR;
		} else if (key.Equals("_type")) {
			data.state = State::TYPE;
		} else if (key.Equals("headers")) {
			data.state = State::HEADERS;
		} else if (key.Equals("entries")) {
			data.state = State::ENTRIES;
			entry_state = State::NONE;
		} else if (key.Equals("url")) {
			data.state = State::URL;
		} else {
			data.state = State::NONE;
		}
	} else if (data.depth == 2 && data.state == State::HEADERS) {
		header_key = key.ToString();
	} else {
		data.state = State::NONE;
	}

	return 1;
}

int
ParserContext::String(StringView value) noexcept
{
	ParserData data = CurrentState();
	StringMetadataTag tag;

	switch (data.state) {
		case State::TITLE:
			tag = StringMetadataTag::TITLE;
			break;
		case State::UPLOAD_DATE:
			tag = StringMetadataTag::UPLOAD_DATE;
			break;
		case State::UPLOADER:
			tag = StringMetadataTag::UPLOADER_NAME;
			break;
		case State::UPLOADER_ID:
			tag = StringMetadataTag::UPLOADER_ID;
			break;
		case State::CREATOR:
			tag = StringMetadataTag::CREATOR;
			break;
		case State::DESCRIPTION:
			tag = StringMetadataTag::DESCRIPTION;
			break;
		case State::PLAYLIST_TITLE:
			tag = StringMetadataTag::PLAYLIST_TITLE;
			break;
		case State::WEBPAGE_URL:
			tag = StringMetadataTag::WEBPAGE_URL;
			break;
		case State::EXTRACTOR:
			tag = StringMetadataTag::EXTRACTOR;
			break;
		case State::TYPE:
			tag = StringMetadataTag::TYPE;
			break;
		case State::URL:
			tag = StringMetadataTag::URL;
			break;
		case State::HEADERS:
			return Result(handler.OnHeader(
				StringView(header_key.c_str(), header_key.size()), value));
		default:
			data.state = State::NONE;
			return 1;
	}

	data.state = State::NONE;
	return Result(handler.OnMetadata(tag, value));
}

int
ParserContext::Integer(long long int value) noexcept
{
	ParserData data = CurrentState();
	IntMetadataTag tag;

	switch (data.state) {
		case State::DURATION:
			tag = IntMetadataTag::DURATION_MS;
			value *= 1000;
			break;
		case State::PLAYLIST_INDEX:
			tag = IntMetadataTag::PLAYLIST_INDEX;
			break;
		default:
			data.state = State::NONE;
			return 1;
	}

	data.state = State::NONE;
	return Result(handler.OnMetadata(tag, value));
}

int
ParserContext::Double(double value) noexcept
{
	ParserData data = CurrentState();

	switch (data.state) {
		case State::DURATION:
			data.state = State::NONE;
			return Result(handler.OnMetadata(IntMetadataTag::DURATION_MS,
				(long long int)(value * 1000)));
		default:
			data.state = State::NONE;
			return 1;
	}
}

using Wrapper = Yajl::CallbacksWrapper<ParserContext>;
static constexpr yajl_callbacks parse_callbacks = {
	nullptr,
	nullptr,
	Wrapper::Integer,
	Wrapper::Double,
	nullptr,
	Wrapper::String,
	Wrapper::StartMap,
	Wrapper::MapKey,
	Wrapper::EndMap,
	Wrapper::StartArray,
	Wrapper::EndArray,
};

Parser::Parser(MetadataHandler &handler) noexcept
	:context(std::make_unique<ParserContext>(handler)) { }

Parser::~Parser() noexcept { }

std::unique_ptr<Yajl::Handle>
Parser::CreateHandle() noexcept
{
	return std::make_unique<Yajl::Handle>(&parse_callbacks, nullptr, context.get());
}

} // namespace Ytdl
