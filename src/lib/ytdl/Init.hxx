#ifndef MPD_LIB_YTDL_INIT_HXX
#define MPD_LIB_YTDL_INIT_HXX

#include "input/InputStream.hxx"
#include <string>
#include <forward_list>

class EventLoop;
class ConfigBlock;

namespace Ytdl {

class YtdlInit {
	EventLoop* event_loop;
	std::forward_list<std::string> domain_whitelist;

public:
	YtdlInit(EventLoop &_event_loop);
	YtdlInit();

	const char *UriSupported(const char *uri) const;
	bool WhitelistMatch(const char *uri) const;

	void Init(const ConfigBlock &block);

	EventLoop &GetEventLoop() { return *event_loop; }
};

} // namespace Ytdl

#endif
