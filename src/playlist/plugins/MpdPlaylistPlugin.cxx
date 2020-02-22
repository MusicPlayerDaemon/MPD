#include "config.h"
#include "MpdPlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../SongEnumerator.hxx"
#include "DetachedSong.hxx"
#include "input/TextInputStream.hxx"
#include "input/InputStream.hxx"
#include "Log.hxx"
#include "util/Domain.hxx"
#include "util/StringStrip.hxx"
#include "Instance.hxx"
#include "external/jaijson/Deserializer.hxx"

static constexpr Domain domain("mpd_playlist");

extern Instance *instance;

class MpdPlaylist final : public SongEnumerator {
	TextInputStream tis;

public:
	MpdPlaylist(InputStreamPtr &&is)
		:tis(std::move(is)) {
	}

	InputStreamPtr CheckFirstLine() {
		char *line = tis.ReadLine();
		if (line == nullptr)
			return tis.StealInputStream();

		StripRight(line);
		if (strcmp(line, "#MPDM3U") != 0)
			return tis.StealInputStream();

		return nullptr;
	}

	virtual std::unique_ptr<DetachedSong> NextSong() override;
};

static std::unique_ptr<SongEnumerator>
mpd_open_stream(InputStreamPtr &&is)
{
	auto playlist = std::make_unique<MpdPlaylist>(std::move(is));

	is = playlist->CheckFirstLine();
	if (is)
		/* no MPDM3U header: fall back to the plain m3u
		   plugin */
		playlist.reset();

	return playlist;
}

std::unique_ptr<DetachedSong>
MpdPlaylist::NextSong()
{
	char *line_s;
	bool fail;
	rapidjson::Document doc;

	do {
		line_s = tis.ReadLine();
		if (line_s == nullptr)
			return nullptr;
		fail = doc.Parse(line_s).HasParseError();
		if (fail) {
			FormatError(domain, "parse %s fail", line_s);
		}
	} while (fail || line_s[0] == '#' || *line_s == 0);

	auto song = std::make_unique<DetachedSong>("");
	deserialize(doc, *song);

	return song;
}

static const char *const mpd_suffixes[] = {
	"m3u",
	nullptr
};

static const char *const mpd_mime_types[] = {
	nullptr
};

const struct playlist_plugin mpd_playlist_plugin = {
	"mpd",

	nullptr,
	nullptr,
	nullptr,
	mpd_open_stream,

	nullptr,
	mpd_suffixes,
	mpd_mime_types,
};
