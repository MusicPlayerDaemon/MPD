#ifndef MPD_LIB_YTDL_PARSER_HXX
#define MPD_LIB_YTDL_PARSER_HXX

#include "lib/yajl/Handle.hxx"
#include "Handler.hxx"
#include <memory>

namespace Ytdl {

class MetadataHandler;
class ParserContext;

class Parser {
	std::unique_ptr<ParserContext> context;

public:
	Parser(MetadataHandler &handler) noexcept;
	~Parser() noexcept;

	std::unique_ptr<Yajl::Handle> CreateHandle() noexcept;
};

enum class PlaylistMode {
	SINGLE,
	FLAT,
	FULL,
};

void Invoke(Yajl::Handle &handle, const char *url, PlaylistMode mode);

} // namespace Ytdl

#endif
