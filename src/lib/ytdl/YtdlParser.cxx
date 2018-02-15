#include "config.h"
#include "YtdlParser.hxx"
#include "tag/TagBuilder.hxx"
#include "util/Alloc.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <yajl/yajl_parse.h>

#include <cinttypes>

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static constexpr Domain ytdl_domain("youtube-dl");

enum YTDL_YAJL_STATE {
	YTDL_YAJL_STATE_NONE = 0,
	YTDL_YAJL_STATE_TITLE,
	YTDL_YAJL_STATE_DURATION,
	YTDL_YAJL_STATE_UPLOAD_DATE,
	YTDL_YAJL_STATE_UPLOADER,
	YTDL_YAJL_STATE_UPLOADER_ID,
	YTDL_YAJL_STATE_CREATOR,
	YTDL_YAJL_STATE_DESCRIPTION,
	YTDL_YAJL_STATE_WEBPAGE_URL,
	YTDL_YAJL_STATE_PLAYLIST_TITLE,
	YTDL_YAJL_STATE_EXTRACTOR,
	YTDL_YAJL_STATE_PLAYLIST_INDEX,
	YTDL_YAJL_STATE_TYPE,
	YTDL_YAJL_STATE_HEADERS,
	YTDL_YAJL_STATE_ENTRIES,
	YTDL_YAJL_STATE_URL,
};

YtdlParseContext::YtdlParseContext() : depth(0), state(0), builder(std::make_unique<TagBuilder>()), playlist_index(0) { }

YtdlParseContext::~YtdlParseContext() { }

int YtdlParseContext::ytdl_yajl_start_array(void *ctx) {
	auto context = (YtdlParseContext*)ctx;
	context->depth++;
	if (context->state == YTDL_YAJL_STATE_ENTRIES && context->depth >= 2) {
		if (context->depth > 2) {
			ytdl_yajl_start_array(context->entry_ptr);
		}
	} else context->state = YTDL_YAJL_STATE_NONE;

	return 1;
}

int YtdlParseContext::ytdl_yajl_end_array(void *ctx) {
	auto context = (YtdlParseContext*)ctx;
	context->depth--;
	if (context->state == YTDL_YAJL_STATE_ENTRIES && context->depth >= 2) {
		ytdl_yajl_end_array(context->entry_ptr);
	} else context->state = YTDL_YAJL_STATE_NONE;

	return 1;
}

int YtdlParseContext::ytdl_yajl_start_map(void *ctx) {
	auto context = (YtdlParseContext*)ctx;
	context->depth++;
	if (context->state == YTDL_YAJL_STATE_HEADERS && context->depth == 2) { }
	else if (context->state == YTDL_YAJL_STATE_ENTRIES && context->depth == 2) {
		return 0; // expected array here
	} else if (context->state == YTDL_YAJL_STATE_ENTRIES && context->depth > 2) {
		if (context->depth == 3) {
			context->entries.emplace_front();
			context->entry_ptr = &context->entries.front();
		}
		ytdl_yajl_start_map(context->entry_ptr);
	} else context->state = YTDL_YAJL_STATE_NONE;

	return 1;
}

int YtdlParseContext::ytdl_yajl_end_map(void *ctx) {
	auto context = (YtdlParseContext*)ctx;
	context->depth--;
	if (context->state == YTDL_YAJL_STATE_ENTRIES && context->depth >= 2) {
		ytdl_yajl_end_map(context->entry_ptr);
	} else context->state = YTDL_YAJL_STATE_NONE;

	return 1;
}

static bool ytdl_yajl_key_match(const unsigned char *key, size_t len, const char* search) {
	return len == strlen(search) && !strncmp((const char*)key, search, len);
}

int YtdlParseContext::ytdl_yajl_map_key(void *ctx, const unsigned char *key, size_t len) {
	auto context = (YtdlParseContext*)ctx;
	if (context->depth == 1) {
		if (ytdl_yajl_key_match(key, len, "title")) {
			context->state = YTDL_YAJL_STATE_TITLE;
		} else if (ytdl_yajl_key_match(key, len, "duration")) {
			context->state = YTDL_YAJL_STATE_DURATION;
		} else if (ytdl_yajl_key_match(key, len, "upload_date")) {
			context->state = YTDL_YAJL_STATE_UPLOAD_DATE;
		} else if (ytdl_yajl_key_match(key, len, "uploader")) {
			context->state = YTDL_YAJL_STATE_UPLOADER;
		} else if (ytdl_yajl_key_match(key, len, "uploader_id")) {
			context->state = YTDL_YAJL_STATE_UPLOADER_ID;
		} else if (ytdl_yajl_key_match(key, len, "creator")) {
			context->state = YTDL_YAJL_STATE_CREATOR;
		} else if (ytdl_yajl_key_match(key, len, "description")) {
			context->state = YTDL_YAJL_STATE_DESCRIPTION;
		} else if (ytdl_yajl_key_match(key, len, "playlist_title")) {
			context->state = YTDL_YAJL_STATE_PLAYLIST_TITLE;
		} else if (ytdl_yajl_key_match(key, len, "webpage_url")) {
			context->state = YTDL_YAJL_STATE_WEBPAGE_URL;
		} else if (ytdl_yajl_key_match(key, len, "extractor_key") || ytdl_yajl_key_match(key, len, "ie_key")) {
			context->state = YTDL_YAJL_STATE_EXTRACTOR;
		} else if (ytdl_yajl_key_match(key, len, "_type")) {
			context->state = YTDL_YAJL_STATE_TYPE;
		} else if (ytdl_yajl_key_match(key, len, "url")) {
			context->state = YTDL_YAJL_STATE_URL;
		} else if (ytdl_yajl_key_match(key, len, "playlist_index")) {
			context->state = YTDL_YAJL_STATE_PLAYLIST_INDEX;
		} else if (ytdl_yajl_key_match(key, len, "headers")) {
			context->state = YTDL_YAJL_STATE_HEADERS;
			context->header_index = 0;
		} else if (ytdl_yajl_key_match(key, len, "entries")) {
			context->state = YTDL_YAJL_STATE_ENTRIES;
			context->entry_ptr = nullptr;
		} else {
			context->state = YTDL_YAJL_STATE_NONE;
		}
	} else if (context->depth == 2 && context->state == YTDL_YAJL_STATE_HEADERS) {
		context->headers.push_back(std::make_pair(std::string((const char*)key, len), std::string()));
		context->header_index = context->headers.size() - 1;
	} else if (context->state == YTDL_YAJL_STATE_ENTRIES && context->depth > 1) {
		if (context->entry_ptr) {
			ytdl_yajl_map_key(context->entry_ptr, key, len);
		} else {
			return 0;
		}
	} else {
		context->state = YTDL_YAJL_STATE_NONE;
	}

	return 1;
}

int YtdlParseContext::ytdl_yajl_string(void *ctx, const unsigned char *value, size_t len) {
	std::string str((const char*)value, len);
	auto context = (YtdlParseContext*)ctx;
	switch (context->state) {
		case YTDL_YAJL_STATE_TITLE: context->builder->AddItem(TAG_TITLE, str.c_str()); break;
		case YTDL_YAJL_STATE_UPLOAD_DATE: context->builder->AddItem(TAG_DATE, str.c_str()); break;
		case YTDL_YAJL_STATE_UPLOADER:
			context->builder->RemoveType(TAG_ARTIST);
			context->builder->AddItem(TAG_ARTIST, str.c_str());
			break;
		case YTDL_YAJL_STATE_UPLOADER_ID:
			if (!context->builder->HasType(TAG_ARTIST)) {
				context->builder->AddItem(TAG_ARTIST, str.c_str());
			}
			break;
		case YTDL_YAJL_STATE_CREATOR:
			context->builder->RemoveType(TAG_ARTIST);
			context->builder->AddItem(TAG_ARTIST, str.c_str());
			break;
		case YTDL_YAJL_STATE_DESCRIPTION: context->builder->AddItem(TAG_COMMENT, str.c_str()); break;
		case YTDL_YAJL_STATE_PLAYLIST_TITLE: context->builder->AddItem(TAG_ALBUM, str.c_str()); break;
		case YTDL_YAJL_STATE_WEBPAGE_URL: context->webpage_url = str; break;
		case YTDL_YAJL_STATE_EXTRACTOR: context->extractor = str; break;
		case YTDL_YAJL_STATE_TYPE: context->type = str; break;
		case YTDL_YAJL_STATE_URL: context->url = str; break;
		case YTDL_YAJL_STATE_HEADERS:
			context->headers[context->header_index].second = str;
			return 1;
		case YTDL_YAJL_STATE_ENTRIES:
			if (context->entry_ptr) {
				ytdl_yajl_string(context->entry_ptr, value, len);
				return 1;
			} else {
				return 0;
			}
		default: break;
	}
	context->state = YTDL_YAJL_STATE_NONE;

	return 1;
}

int YtdlParseContext::ytdl_yajl_integer(void *ctx, long long int value) {
	auto context = (YtdlParseContext*)ctx;
	switch (context->state) {
		case YTDL_YAJL_STATE_DURATION: context->builder->SetDuration(SignedSongTime::FromS((int)value)); break;
		case YTDL_YAJL_STATE_PLAYLIST_INDEX: context->playlist_index = (int)value; break;
		case YTDL_YAJL_STATE_ENTRIES:
			if (context->entry_ptr) {
				ytdl_yajl_integer(context->entry_ptr, value);
				return 1;
			} else {
				return 0;
			}
		default: break;
	}
	context->state = YTDL_YAJL_STATE_NONE;

	return 1;
}

int YtdlParseContext::ytdl_yajl_double(void *ctx, double value) {
	auto context = (YtdlParseContext*)ctx;
	switch (context->state) {
		case YTDL_YAJL_STATE_DURATION: context->builder->SetDuration(SignedSongTime::FromMS((int)(value * 1000))); break;
		case YTDL_YAJL_STATE_ENTRIES:
			if (context->entry_ptr) {
				ytdl_yajl_double(context->entry_ptr, value);
				return 1;
			} else {
				return 0;
			}
		default: return ytdl_yajl_integer(ctx, (long long int)value);
	}
	context->state = YTDL_YAJL_STATE_NONE;

	return 1;
}

const void* YtdlParseContext::YajlCallbacks() {
	static constexpr yajl_callbacks ytdl_yajl_callbacks = {
		nullptr,
		nullptr,
		YtdlParseContext::ytdl_yajl_integer,
		YtdlParseContext::ytdl_yajl_double,
		nullptr,
		YtdlParseContext::ytdl_yajl_string,
		YtdlParseContext::ytdl_yajl_start_map,
		YtdlParseContext::ytdl_yajl_map_key,
		YtdlParseContext::ytdl_yajl_end_map,
		YtdlParseContext::ytdl_yajl_start_array,
		YtdlParseContext::ytdl_yajl_end_array,
	};

	return &ytdl_yajl_callbacks;
}

bool YtdlParseJson(YtdlParseContext *context, const char *url, YTDL_PLAYLIST_MODE playlist_mode) {
	yajl_handle handle = nullptr;

	int pid = 0;
	int pipefd[2] = {-1, -1};
	bool result = false;
	int ret;
	yajl_status status;
	uint8_t buffer[0x80];

	if (pipe(pipefd) < 0) {
		goto fail;
	}

	pid = fork();

	if (pid < 0) {
		goto fail;
	}

	if (!pid) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		const char *playlist;
		switch (playlist_mode) {
			case YTDL_PLAYLIST_MODE_SINGLE:
				playlist = "--no-playlist";
				break;
			case YTDL_PLAYLIST_MODE_FLAT:
				playlist = "--flat-playlist";
				break;
			case YTDL_PLAYLIST_MODE_FULL:
			default:
				playlist = "--yes-playlist";
				break;
		}
		if (execlp("youtube-dl", "youtube-dl", "-Jf", "bestaudio/best", playlist, url, nullptr) < 0) {
			close(pipefd[1]);
			_exit(EXIT_FAILURE);
		}
	}

	close(pipefd[1]);
	pipefd[1] = -1;

	LogDebug(ytdl_domain, "Parsing JSON");

	handle = yajl_alloc((const yajl_callbacks*)YtdlParseContext::YajlCallbacks(), nullptr, context);
	do {
		ret = read(pipefd[0], buffer, sizeof(buffer));
		if (ret > 0) {
			status = yajl_parse(handle, buffer, ret);
		}
	} while (ret > 0 && status == yajl_status_ok);
	if (ret < 0 || status != yajl_status_ok) {
		goto fail;
	}

	if (yajl_complete_parse(handle) != yajl_status_ok) {
		goto fail;
	}

	result = true;

fail:
	for (int i = 0; i < 2; i++) {
		if (pipefd[i] >= 0) {
			close(pipefd[i]);
		}
	}
	if (handle) {
		yajl_free(handle);
	}
	if (pid > 0) {
		if (waitpid(pid, &ret, 0) < 0 || ret) {
			result = false;
		}
	}
	return result;
}
