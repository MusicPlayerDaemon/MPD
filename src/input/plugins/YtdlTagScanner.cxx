#include "config.h"
#include "YtdlTagScanner.hxx"
#include "lib/ytdl/TagHandler.hxx"
#include "lib/ytdl/Parser.hxx"
#include "tag/Tag.hxx"


YtdlTagScanner::YtdlTagScanner(EventLoop &_event_loop, const std::string &_uri, RemoteTagHandler &_handler)
	:event_loop(_event_loop), uri(_uri), handler(_handler) { }

YtdlTagScanner::~YtdlTagScanner() { }

void
YtdlTagScanner::Start()
{
	assert(context == nullptr);
	try {
		context = Ytdl::InvokeContext::Invoke(uri.c_str(), Ytdl::PlaylistMode::SINGLE, event_loop, *this);
	} catch (...) {
		handler.OnRemoteTagError(std::current_exception());
	}
}

void YtdlTagScanner::OnComplete(Ytdl::YtdlMonitor* monitor) {
	handler.OnRemoteTag(context->GetMetadata().GetTagBuilder().Commit());
}

void YtdlTagScanner::OnError(Ytdl::YtdlMonitor* monitor, std::exception_ptr e) {
	handler.OnRemoteTagError(e);
}
