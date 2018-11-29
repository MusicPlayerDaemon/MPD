#include "config.h"
#include "YtdlTagScanner.hxx"
#include "lib/ytdl/TagHandler.hxx"
#include "lib/ytdl/Parser.hxx"
#include "tag/Tag.hxx"

struct YtdlTagScannerContext {
	std::unique_ptr<Ytdl::TagHandler> metadata;
	std::unique_ptr<Ytdl::Parser> parser;
	std::unique_ptr<Yajl::Handle> handle;
	std::unique_ptr<Ytdl::YtdlMonitor> monitor;

	YtdlTagScannerContext(
		std::unique_ptr<Ytdl::TagHandler> &&metadata_,
		std::unique_ptr<Ytdl::Parser> &&parser_,
		std::unique_ptr<Yajl::Handle> &&handle_,
		std::unique_ptr<Ytdl::YtdlMonitor> &&monitor_
	) :
		metadata(std::move(metadata_)),
		parser(std::move(parser_)),
		handle(std::move(handle_)),
		monitor(std::move(monitor_))
	{ }

};

YtdlTagScanner::YtdlTagScanner(EventLoop &_event_loop, const std::string &_uri, RemoteTagHandler &_handler)
	:event_loop(_event_loop), uri(_uri), handler(_handler) { }

YtdlTagScanner::~YtdlTagScanner() { }

void
YtdlTagScanner::Start()
{
	assert(context == nullptr);
	try {
		auto metadata = std::make_unique<Ytdl::TagHandler>();
		auto parser = std::make_unique<Ytdl::Parser>(*metadata);
		auto handle = parser->CreateHandle();
		auto monitor = Ytdl::Invoke(*handle, uri.c_str(), Ytdl::PlaylistMode::SINGLE, event_loop, *this);
		context = std::make_unique<YtdlTagScannerContext>(std::move(metadata), std::move(parser), std::move(handle), std::move(monitor));
	} catch (...) {
		handler.OnRemoteTagError(std::current_exception());
	}
}

void YtdlTagScanner::OnComplete(Ytdl::YtdlMonitor* monitor) {
	handler.OnRemoteTag(context->metadata->GetTagBuilder().Commit());
}

void YtdlTagScanner::OnError(Ytdl::YtdlMonitor* monitor, std::exception_ptr e) {
	handler.OnRemoteTagError(e);
}
