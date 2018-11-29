#ifndef MPD_LIB_YTDL_INVOKE_HXX
#define MPD_LIB_YTDL_INVOKE_HXX

#include "Parser.hxx"
#include "TagHandler.hxx"

namespace Ytdl {

class InvokeContext {
	std::unique_ptr<TagHandler> metadata;
	std::unique_ptr<Parser> parser;
	std::unique_ptr<Yajl::Handle> handle;
	std::unique_ptr<YtdlMonitor> monitor;

public:
	InvokeContext(
		std::unique_ptr<TagHandler> &&metadata_,
		std::unique_ptr<Parser> &&parser_,
		std::unique_ptr<Yajl::Handle> &&handle_,
		std::unique_ptr<YtdlMonitor> &&monitor_
	) :
		metadata(std::move(metadata_)),
		parser(std::move(parser_)),
		handle(std::move(handle_)),
		monitor(std::move(monitor_))
	{ }

	static std::unique_ptr<InvokeContext>
	Invoke(const char* uri, PlaylistMode mode, EventLoop &event_loop, YtdlHandler &handler);

	TagHandler &GetMetadata() { return *metadata; }
};

} // namespace Ytdl

#endif
