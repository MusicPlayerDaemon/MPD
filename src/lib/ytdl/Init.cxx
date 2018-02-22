#include "config.h"
#include "Init.hxx"
#include "config/Block.hxx"
#include "util/StringView.hxx"
#include "util/StringCompare.hxx"
#include "util/IterableSplitString.hxx"

static const char* DEFAULT_WHITELIST =
	"www.youtube.com "
	"www.soundcloud.com "
	"www.bandcamp.com "
	"www.twitch.tv";

namespace Ytdl {

YtdlInit::YtdlInit(EventLoop &_event_loop): event_loop(&_event_loop) { }

YtdlInit::YtdlInit(): event_loop(nullptr) { }

const char *
YtdlInit::UriSupported(const char *uri) const
{
	const char* p;

	if ((p = StringAfterPrefix(uri, "youtube-dl://"))) {
		return p;
	} else if (WhitelistMatch(uri)) {
		return uri;
	} else {
		return nullptr;
	}
}

bool
YtdlInit::WhitelistMatch(const char *uri) const
{
	const char* p;
	if (!(p = StringAfterPrefix(uri, "http://")) &&
		!(p = StringAfterPrefix(uri, "https://"))) {
		return false;
	}

	StringView domain(p);
	for (const auto &whitelist : domain_whitelist) {
		if (domain.StartsWith(whitelist.c_str())) {
			return true;
		}
	}

	return false;
}

void
YtdlInit::Init(const ConfigBlock &block)
{
	const char* domains = block.GetBlockValue("domain_whitelist", DEFAULT_WHITELIST);

	for (const auto domain : IterableSplitString(domains, ' ')) {
		if (!domain.empty()) {
			domain_whitelist.emplace_front(domain.ToString());
		}
	}
}

} // namespace Ytdl
