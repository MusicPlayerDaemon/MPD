#ifndef MPD_LIB_YTDL_PARSER_HXX
#define MPD_LIB_YTDL_PARSER_HXX

#include <stddef.h>
#include <string>
#include <vector>
#include <forward_list>
#include <memory>
#include <utility>
#include <map>

class TagBuilder;

class YtdlParseContext {
private:
	int depth;
	int state; // YTDL_YAJL_STATE
	int header_index;
	YtdlParseContext* entry_ptr;

	static int ytdl_yajl_start_array(void *ctx);
	static int ytdl_yajl_end_array(void *ctx);
	static int ytdl_yajl_start_map(void *ctx);
	static int ytdl_yajl_end_map(void *ctx);
	static int ytdl_yajl_map_key(void *ctx, const unsigned char *key, size_t len);
	static int ytdl_yajl_string(void *ctx, const unsigned char *value, size_t len);
	static int ytdl_yajl_integer(void *ctx, long long int value);
	static int ytdl_yajl_double(void *ctx, double value);

public:
	std::unique_ptr<TagBuilder> builder;
	std::forward_list<YtdlParseContext> entries;
	std::vector<std::pair<std::string, std::string>> headers;
	std::string url;
	std::string webpage_url;
	std::string type;
	std::string extractor;
	int playlist_index;

	YtdlParseContext();
	~YtdlParseContext();

	static const void* YajlCallbacks();
};

enum YTDL_PLAYLIST_MODE {
	YTDL_PLAYLIST_MODE_SINGLE,
	YTDL_PLAYLIST_MODE_FLAT,
	YTDL_PLAYLIST_MODE_FULL,
};

struct YtdlPlaylistSort {
	inline bool operator() (const YtdlParseContext& con0, const YtdlParseContext& con1)
	{
		return con0.playlist_index < con1.playlist_index;
	}
};

bool YtdlParseJson(YtdlParseContext *context, const char *url, YTDL_PLAYLIST_MODE playlist_mode);

#endif
