#include "CdioParanoiaMusicBrainzXMLParser.hxx"
#include "lib/expat/ExpatParser.hxx"

struct MBZParser
{
	enum
	{
		ROOT,
		RELEASE,
		RELEASE_TITLE,
		RELEASE_ARTIST_BLOCK,
		RELEASE_ARTIST_NAME,
		RELEASE_ARTIST_GENRE,
		RELEASE_ARTIST_GENRE_NAME,
		RELEASE_DATE,
		TRACK_LIST,
		TRACK,
		RECORDING_TRACKNUM,
		RECORDING_TITLE,
		RECORDING_DURATION,
		RECORDING_ARTIST_BLOCK,
		RECORDING_ARTIST_NAME,
		RECORDING_ARTIST_GENRE,
		RECORDING_FIRST_RELEASE_DATE,
	} state = ROOT;

	std::string value;

	MusicBrainzCDTagCache::TrackInfo currentTrack;
	std::map<int, MusicBrainzCDTagCache::TrackInfo> tracks;

	std::string albumTitle;
	std::string albumDate;
	std::string albumArtist;
	std::string albumGenre;

	void finishCurrenTrack ()
	{
		currentTrack.albumTitle = albumTitle;
		currentTrack.albumDate = albumDate;
		currentTrack.albumArtist = albumArtist;
		currentTrack.albumGenre = albumGenre;
		tracks[currentTrack.trackNum] = currentTrack;
		currentTrack = {};
	}
};

static void XMLCALL
mbz_start_element (void *user_data, const XML_Char *element_name,
		[[maybe_unused]] const XML_Char **atts)
{
	auto *parser = (MBZParser *)user_data;

	parser->value.clear();

	switch (parser->state)
	{
	case MBZParser::ROOT:
		if (strcmp(element_name, "release") == 0)
			parser->state = MBZParser::RELEASE;
		break;
	case MBZParser::RELEASE:
		if (strcmp(element_name, "artist") == 0)
			parser->state = MBZParser::RELEASE_ARTIST_BLOCK;
		else if (strcmp(element_name, "title") == 0)
			parser->state = MBZParser::RELEASE_TITLE;
		else if (strcmp(element_name, "date") == 0)
			parser->state = MBZParser::RELEASE_DATE;
		else if (strcmp(element_name, "track-list") == 0)
			parser->state = MBZParser::TRACK_LIST;
		break;
	case MBZParser::RELEASE_ARTIST_BLOCK:
		if (strcmp(element_name, "name") == 0)
			parser->state = MBZParser::RELEASE_ARTIST_NAME;
		else if (strcmp(element_name, "genre-list") == 0)
			parser->state = MBZParser::RELEASE_ARTIST_GENRE;
		break;
	case MBZParser::RELEASE_ARTIST_GENRE:
		if (strcmp(element_name, "name") == 0)
			parser->state = MBZParser::RELEASE_ARTIST_GENRE_NAME;
		break;
	case MBZParser::TRACK_LIST:
		if (strcmp(element_name, "track") == 0)
			parser->state = MBZParser::TRACK;
		break;
	case MBZParser::TRACK:
		if (strcmp(element_name, "artist") == 0)
			parser->state = MBZParser::RECORDING_ARTIST_BLOCK;
		else if (strcmp(element_name, "title") == 0)
			parser->state = MBZParser::RECORDING_TITLE;
		else if (strcmp(element_name, "length") == 0)
			parser->state = MBZParser::RECORDING_DURATION;
		else if (strcmp(element_name, "number") == 0)
			parser->state = MBZParser::RECORDING_TRACKNUM;
		else if (strcmp(element_name, "first-release-date") == 0)
			parser->state = MBZParser::RECORDING_FIRST_RELEASE_DATE;
		break;
	case MBZParser::RECORDING_ARTIST_BLOCK:
		if (strcmp(element_name, "name") == 0)
			parser->state = MBZParser::RECORDING_ARTIST_NAME;
		else if (strcmp(element_name, "genre-list") == 0)
			parser->state = MBZParser::RECORDING_ARTIST_GENRE;
		break;
	case MBZParser::RECORDING_ARTIST_GENRE:
	case MBZParser::RELEASE_TITLE:
	case MBZParser::RELEASE_ARTIST_NAME:
	case MBZParser::RELEASE_ARTIST_GENRE_NAME:
	case MBZParser::RELEASE_DATE:
	case MBZParser::RECORDING_TRACKNUM:
	case MBZParser::RECORDING_TITLE:
	case MBZParser::RECORDING_DURATION:
	case MBZParser::RECORDING_ARTIST_NAME:
	case MBZParser::RECORDING_FIRST_RELEASE_DATE:
		break;
	}
}

static void XMLCALL
mbz_end_element (void *user_data, const XML_Char *element_name)
{
	auto *parser = (MBZParser *)user_data;

	switch (parser->state)
	{
	case MBZParser::ROOT:
		break;
	case MBZParser::RELEASE:
		if (strcmp(element_name, "release") == 0)
			parser->state = MBZParser::ROOT;
		break;
	case MBZParser::RELEASE_TITLE:
		parser->state = MBZParser::RELEASE;
		parser->albumTitle = parser->value;
		break;
	case MBZParser::RELEASE_ARTIST_BLOCK:
		if (strcmp(element_name, "artist") == 0)
			parser->state = MBZParser::RELEASE;
		break;
	case MBZParser::RELEASE_ARTIST_NAME:
		parser->albumArtist = parser->value;
		parser->state = MBZParser::RELEASE_ARTIST_BLOCK;
		break;
	case MBZParser::RELEASE_ARTIST_GENRE:
		if (strcmp(element_name, "genre-list") == 0)
			parser->state = MBZParser::RELEASE_ARTIST_BLOCK;
		break;
	case MBZParser::RELEASE_ARTIST_GENRE_NAME:
		if (parser->albumGenre.length() > 0)
			parser->albumGenre += ",";
		parser->albumGenre += parser->value;
		parser->state = MBZParser::RELEASE_ARTIST_GENRE;
		break;
	case MBZParser::RELEASE_DATE:
		parser->albumDate = parser->value;
		parser->state = MBZParser::RELEASE;
		break;
	case MBZParser::TRACK_LIST:
		if (strcmp(element_name, "track-list") == 0)
			parser->state = MBZParser::RELEASE;
		break;
	case MBZParser::TRACK:
		if (strcmp(element_name, "track") == 0)
		{
			parser->finishCurrenTrack();
			parser->state = MBZParser::TRACK_LIST;
		}
		break;
	case MBZParser::RECORDING_TRACKNUM:
		parser->currentTrack.trackNum = std::stoi(parser->value);
		parser->state = MBZParser::TRACK;
		break;
	case MBZParser::RECORDING_TITLE:
		parser->currentTrack.title = parser->value;
		parser->state = MBZParser::TRACK;
		break;
	case MBZParser::RECORDING_DURATION:
		parser->currentTrack.duration = (std::stoi(parser->value) + 500) / 1000;
		parser->state = MBZParser::TRACK;
		break;
	case MBZParser::RECORDING_ARTIST_BLOCK:
		if (strcmp(element_name, "artist") == 0)
			parser->state = MBZParser::TRACK;
		break;
	case MBZParser::RECORDING_ARTIST_GENRE:
		if (strcmp(element_name, "genre-list") == 0)
			parser->state = MBZParser::RECORDING_ARTIST_BLOCK;
		break;
	case MBZParser::RECORDING_ARTIST_NAME:
		parser->currentTrack.artist = parser->value;
		parser->state = MBZParser::RECORDING_ARTIST_BLOCK;
		break;
	case MBZParser::RECORDING_FIRST_RELEASE_DATE:
		parser->currentTrack.firstReleaseDate = parser->value;
		parser->state = MBZParser::TRACK;
		break;
	}
}

static void XMLCALL
mbz_char_data (void *user_data, const XML_Char *s, int len)
{
	auto *parser = (MBZParser *)user_data;

	switch (parser->state)
	{
	case MBZParser::ROOT:
	case MBZParser::RELEASE:
	case MBZParser::RELEASE_ARTIST_BLOCK:
	case MBZParser::TRACK_LIST:
	case MBZParser::TRACK:
	case MBZParser::RECORDING_ARTIST_BLOCK:
	case MBZParser::RECORDING_ARTIST_GENRE:
	case MBZParser::RELEASE_ARTIST_GENRE:
		break;
	case MBZParser::RELEASE_TITLE:
	case MBZParser::RELEASE_ARTIST_NAME:
	case MBZParser::RECORDING_ARTIST_NAME:
	case MBZParser::RECORDING_TITLE:
	case MBZParser::RECORDING_DURATION:
	case MBZParser::RELEASE_DATE:
	case MBZParser::RECORDING_FIRST_RELEASE_DATE:
	case MBZParser::RECORDING_TRACKNUM:
		parser->value.append(s, len);
		break;
	case MBZParser::RELEASE_ARTIST_GENRE_NAME:
		if (parser->value.length() > 0)
			parser->value += ",";
		parser->value.append(s, len);
		break;
	}
}

std::map<int, MusicBrainzCDTagCache::TrackInfo>
MusicBrainzXMLParser::parse(std::string& body)
{
	MBZParser mbzParser;
	{
		ExpatParser expat(&mbzParser);
	
		expat.SetElementHandler(mbz_start_element, mbz_end_element);
		expat.SetCharacterDataHandler(mbz_char_data);
		expat.Parse(body, true);
	}
	return mbzParser.tracks;
}
