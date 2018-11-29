#ifndef MPD_INPUT_YTDL_TAG_SCANNER_HXX
#define MPD_INPUT_YTDL_TAG_SCANNER_HXX

#include "../RemoteTagScanner.hxx"
#include "lib/ytdl/Parser.hxx"
#include "lib/ytdl/Invoke.hxx"
#include <string>

class YtdlTagScanner: public RemoteTagScanner, public Ytdl::YtdlHandler {
	EventLoop &event_loop;
	std::string uri;
	RemoteTagHandler &handler;
	std::unique_ptr<Ytdl::InvokeContext> context;

public:
	YtdlTagScanner(EventLoop &event_loop, const std::string &uri, RemoteTagHandler &handler);

	~YtdlTagScanner() override;

	void Start() override;

	void OnComplete(Ytdl::YtdlMonitor* monitor);
	void OnError(Ytdl::YtdlMonitor* monitor, std::exception_ptr e);
};

#endif
