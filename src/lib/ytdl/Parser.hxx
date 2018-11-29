#ifndef MPD_LIB_YTDL_PARSER_HXX
#define MPD_LIB_YTDL_PARSER_HXX

#include "event/SocketMonitor.hxx"
#include "lib/yajl/Handle.hxx"
#include "Handler.hxx"
#include <memory>

namespace Ytdl {

class MetadataHandler;
class ParserContext;
class YtdlMonitor;

enum class PlaylistMode {
	SINGLE,
	FLAT,
	FULL,
};

class Parser {
	std::unique_ptr<ParserContext> context;

public:
	Parser(MetadataHandler &handler) noexcept;
	~Parser() noexcept;

	std::unique_ptr<Yajl::Handle> CreateHandle() noexcept;
};

class YtdlHandler {
public:
	virtual void OnComplete(YtdlMonitor* monitor) = 0;
	virtual void OnError(YtdlMonitor* monitor, std::exception_ptr e) = 0;
};

class YtdlProcess {
	Yajl::Handle &handle;
	int fd;
	int pid;

public:
	YtdlProcess(Yajl::Handle &_handle, int _fd, int _pid) noexcept
		:handle(_handle), fd(_fd), pid(_pid) {}

	~YtdlProcess();

	int GetDescriptor() const noexcept {
		return fd;
	}

	static std::unique_ptr<YtdlProcess> Invoke(Yajl::Handle &handle, const char *url, PlaylistMode mode);
	bool Process();
};

class YtdlMonitor : public SocketMonitor {
	YtdlHandler &handler;
	std::unique_ptr<YtdlProcess> process;

public:
	YtdlMonitor(YtdlHandler &_handler, std::unique_ptr<YtdlProcess> && _process, EventLoop &_loop) noexcept
		:SocketMonitor(SocketDescriptor(_process->GetDescriptor()), _loop), handler(_handler), process(std::move(_process)) {}

protected:
	bool OnSocketReady(unsigned flags) noexcept;
};

void BlockingInvoke(Yajl::Handle &handle, const char *url, PlaylistMode mode);

std::unique_ptr<YtdlMonitor>
Invoke(Yajl::Handle &handle, const char *url, PlaylistMode mode, EventLoop &loop, YtdlHandler &handler);

} // namespace Ytdl

#endif
