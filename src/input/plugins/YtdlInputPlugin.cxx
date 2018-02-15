#include "config.h"
#include "lib/ytdl/YtdlParser.hxx"
#include "YtdlInputPlugin.hxx"
#include "CurlInputPlugin.hxx"
#include "../AsyncInputStream.hxx"
#include "../ProxyInputStream.hxx"
#include "../InputPlugin.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/Block.hxx"
#include "tag/TagBuilder.hxx"
#include "tag/Tag.hxx"
#include "event/Call.hxx"
#include "event/Loop.hxx"
#include "thread/Cond.hxx"
#include "util/Alloc.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <cinttypes>

#include <assert.h>
#include <string.h>

static constexpr Domain ytdl_domain("youtube-dl");

static void
input_ytdl_init(const ConfigBlock &block)
{
	// TODO: a configuration should allow overriding the curl input plugin
	// this plugin would then accept http/https URIs as well
}

static void
input_ytdl_finish() noexcept
{
}

struct YtdlUriFlags {
	bool notags;

	YtdlUriFlags() : notags(false) { }
};

static std::string ytdl_uri(const char *uri, YtdlUriFlags* flags) {
	std::string url;
	if (!strncmp(uri, "youtube-dl://", 13)) {
		uri += 13;
		if (!strncmp(uri, "notag/", 6)) {
			flags->notags = true;
			uri += 6;
		}
		url = uri;
	}

	return url;
}


static InputStream *input_ytdl_open(const char *uri, Mutex &mutex, Cond &cond)
{
	InputStream *res = nullptr;

	if (!strncmp(uri, "youtube-dl://", 13)) {
		YtdlParseContext context;
		YtdlUriFlags flags;
		std::string url = ytdl_uri(uri, &flags);
		if (!url.empty() && YtdlParseJson(&context, url.c_str(), YTDL_PLAYLIST_MODE_SINGLE)) {
			if (context.extractor.empty()) {
				context.extractor = "youtube-dl";
			}
			if (!context.builder->HasType(TAG_ALBUM)) {
				context.builder->AddItem(TAG_ALBUM, context.extractor.c_str());
			}
			context.builder->AddItem(TAG_COMMENT, context.webpage_url.c_str());
			res = OpenCurlInputStream(context.url.c_str(), /* context.headers, */ mutex, cond, flags.notags ? nullptr : context.builder->CommitNew());
		}
	}

	return res;
}

static constexpr const char *ytdl_prefixes[] = {
	"youtube-dl://",
	nullptr
};

const struct InputPlugin input_plugin_ytdl = {
	"youtube-dl",
	ytdl_prefixes,
	input_ytdl_init,
	input_ytdl_finish,
	input_ytdl_open,
};
