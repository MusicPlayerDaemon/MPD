#pragma once

#include "CdioParanoiaMusicBrainzTags.hxx"

class MusicBrainzXMLParser
{
public:
	std::map<int, MusicBrainzCDTagCache::TrackInfo> parse(std::string& body);
};
