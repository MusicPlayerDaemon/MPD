#include "playlist/cue/CueParser.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringView.hxx"

extern "C" {
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	CueParser parser;

	const std::string_view src{(const char *)data, size};

	for (const auto line : IterableSplitString(src, '\n')) {
		parser.Feed(line);
		parser.Get();
	}

	parser.Finish();
	parser.Get();

	return 0;
}
