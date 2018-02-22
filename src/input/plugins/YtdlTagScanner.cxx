#include "config.h"
#include "YtdlTagScanner.hxx"
#include "lib/ytdl/TagHandler.hxx"
#include "lib/ytdl/Parser.hxx"
#include "tag/Tag.hxx"

YtdlTagScanner::YtdlTagScanner(const std::string &_uri, RemoteTagHandler &_handler)
	:uri(_uri), handler(_handler) { }

YtdlTagScanner::~YtdlTagScanner() { }

void
YtdlTagScanner::Start()
{
	try {
		Ytdl::TagHandler metadata;
		Ytdl::Parser parser(metadata);
		auto handle = parser.CreateHandle();
		Ytdl::Invoke(*handle, uri.c_str(), Ytdl::PlaylistMode::SINGLE);
		handler.OnRemoteTag(metadata.builder->Commit());
	} catch (...) {
		handler.OnRemoteTagError(std::current_exception());
	}
}
