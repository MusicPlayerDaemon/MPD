#include "CdioParanoiaMusicBrainzTags.hxx"
#include "CdioParanoiaCDID.hxx"
#include "CdioParanoiaMusicBrainzXMLParser.hxx"

MusicBrainzCDTagCache* MusicBrainzCDTagCache::instance = nullptr;

void 
MusicBrainzCDTagCache::requestTags (std::string_view& uri, std::string device_, Listener *listener)
{
	bool callListener = false;
	int trackNum;

	device = device_;

	{
		const std::scoped_lock lock{mutex};
		const char* uriPrefix = "cdda:///";

		if (strlen(uri.data()) <= strlen(uriPrefix))
			return ;

		trackNum = atoi(uri.data() + strlen(uriPrefix));

		if (insertedCdChanged())
		{
			clearTracks();
			requestMusicBrainzTags();
		}
		callListener = dataReady;
		if (!dataReady)
			listeners[trackNum].insert(listener);
	}

	if (callListener)
	{
		auto it = tracks.find(trackNum);

		if (it != tracks.end())
			listener->setTags(it->second);
	}
}

void
MusicBrainzCDTagCache::cancelRequest (Listener* listener)
{
	const std::scoped_lock lock{mutex};

	for (auto it : listeners)
	{
		auto lit = it.second.find(listener);

		if (lit != it.second.end())
			it.second.erase(lit);
	}
}

bool
MusicBrainzCDTagCache::insertedCdChanged ()
{
	auto cdId = CDIODiscID::getCurrentCDId(device);

	if (cdId.length() == 0)
	{
		if (lastCdId.length() > 0)
		{
			lastCdId = {};
			return true;
		}
		return false;
	}

	if (cdId == lastCdId)
		return false;

	lastCdId = cdId;
	return true;
}

void
MusicBrainzCDTagCache::clearTracks ()
{
	dataReady = false;
	tracks.clear();
	if (request.get() != nullptr)
		request->StopIndirect();
	responseHandler.reset(nullptr);
}

void
MusicBrainzCDTagCache::requestMusicBrainzTags()
{
	if (lastCdId.length() == 0)
		return ;

	// create url
	std::string urlPrefix("https://musicbrainz.org/ws/2/discid/");
	std::string urlArgs("?inc=artist-credits+recordings+genres");
	std::string url = urlPrefix + lastCdId + urlArgs;

	// launch curl request
	responseHandler.reset(new ResponseHandler());
	request.reset(new CurlRequest(*curl, url.c_str(),
				*(StringCurlResponseHandler*)responseHandler.get()));

	request->StartIndirect();
}

bool
MusicBrainzCDTagCache::makeTrackInfoFromXml (std::string& body)
{
	MusicBrainzXMLParser parser;

	tracks = parser.parse(body);

	return tracks.size() > 0;
}

void
MusicBrainzCDTagCache::callListeners ()
{
	const std::scoped_lock lock{mutex};

	dataReady = true;
	for (auto it : listeners)
	{
		int trackNum = it.first;
		auto trackInfoIt = tracks.find(trackNum);

		if (trackInfoIt == tracks.end())
		{
			continue;
		}

		for (auto listener : it.second)
		{
			listener->setTags(trackInfoIt->second);
		}
	}
	listeners.clear();
}
