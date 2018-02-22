#ifndef MPD_INPUT_YTDL_TAG_SCANNER_HXX
#define MPD_INPUT_YTDL_TAG_SCANNER_HXX

#include "../RemoteTagScanner.hxx"
#include <string>

class YtdlTagScanner: public RemoteTagScanner {
	std::string uri;
	RemoteTagHandler &handler;

public:
	YtdlTagScanner(const std::string &uri, RemoteTagHandler &handler);

	~YtdlTagScanner() override;

	void Start() override;
};

#endif
