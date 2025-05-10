#pragma once

#include "CdioParanoiaMusicBrainzTags.hxx"
#include <map>

std::map<int, MusicBrainzCDTagCache::TrackInfo> musicBrainzXMLParser (std::string& body);
