#pragma once

#include "lib/curl/Init.hxx"
#include "lib/curl/Headers.hxx"
#include "lib/curl/Request.hxx"
#include "lib/curl/StringHandler.hxx"
#include "input/RemoteTagScanner.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"

#include <map>
#include <set>

class MusicBrainzCDTagCache
{
public:
	MusicBrainzCDTagCache (EventLoop &event_loop)
	: curl(event_loop)
	{
	}

	virtual ~MusicBrainzCDTagCache ()
	{
		clearTracks();
	}

public:
	struct TrackInfo
	{
		int trackNum = -99;
		std::string title;
		std::string artist;
		std::string firstReleaseDate;
		std::string albumTitle;
		std::string albumDate;
		std::string albumArtist;
		std::string albumGenre;
		int duration = 0;
	};

public:
	class Listener
	{
	public: // MusicBrainzCDTagCache::Listener
		virtual void setTags (MusicBrainzCDTagCache::TrackInfo& trackInfo) = 0;
	};

public:
	void requestTags (std::string_view& uri, std::string device_, Listener *listener);
	void cancelRequest (Listener* listener);
	bool insertedCdChanged ();
	void clearTracks ();
	void requestMusicBrainzTags();
	bool makeTrackInfoFromXml (std::string& body);
	void callListeners ();

public:
	static void deleteInstance ()
	{
		if (instance != nullptr)
			delete instance;
		instance = nullptr;
	}

	static void createInstance (EventLoop &event_loop)
	{
		instance = new MusicBrainzCDTagCache(event_loop);
	}

	static MusicBrainzCDTagCache* getInstance ()
	{
		return instance;
	}

private:
	std::string lastCdId;
	std::map<int, TrackInfo> tracks;

	static MusicBrainzCDTagCache *instance;

	std::map<int, std::set<Listener*> > listeners;
	Mutex mutex;
	CurlInit curl;
	std::unique_ptr<CurlRequest> request;
	bool dataReady = false;
	std::string device;

	class ResponseHandler
	: public StringCurlResponseHandler
	{
	public: // CurlResponseHandler
		/* virtual methods from CurlResponseHandler */
		void OnEnd() override
		{
			auto resp = StringCurlResponseHandler::GetResponse();

			if (MusicBrainzCDTagCache::getInstance()->makeTrackInfoFromXml(resp.body))
				MusicBrainzCDTagCache::getInstance()->callListeners();
		}

		void OnError(std::exception_ptr ) noexcept override
		{
		}

	};

	std::unique_ptr<ResponseHandler> responseHandler;
};

class MusicBrainzTagScanner final
: public RemoteTagScanner
, public MusicBrainzCDTagCache::Listener
{
	RemoteTagHandler &handler;
	std::string_view uri;
	std::string device;
	bool tagsset = false;

public:
	MusicBrainzTagScanner(std::string_view _uri,
			RemoteTagHandler &_handler,
			std::string device_
			)
	: handler(_handler)
	, uri(_uri)  
	, device(device_)
	{
	}

	~MusicBrainzTagScanner() noexcept override
	{
		if (!tagsset)
			MusicBrainzCDTagCache::getInstance()->cancelRequest(this);
	}

public: // MusicBrainzCDTagCache::Listener
	virtual void setTags (MusicBrainzCDTagCache::TrackInfo& trackInfo) override
	{
		TagBuilder b;
		Tag tag;

		b.AddItem(TAG_TITLE, trackInfo.title);
		b.AddItem(TAG_ARTIST, trackInfo.artist);
		b.AddItem(TAG_ORIGINAL_DATE, trackInfo.firstReleaseDate);
		b.AddItem(TAG_ALBUM, trackInfo.albumTitle);
		if (trackInfo.albumDate == std::string())
			b.AddItem(TAG_DATE, trackInfo.firstReleaseDate);
		else
			b.AddItem(TAG_DATE, trackInfo.albumDate);
		b.AddItem(TAG_ALBUM_ARTIST, trackInfo.albumArtist);
		b.AddItem(TAG_GENRE, trackInfo.albumGenre);
		b.AddItem(TAG_TRACK, std::to_string(trackInfo.trackNum));
		b.SetDuration(SignedSongTime::FromS(trackInfo.duration));
		b.Commit(tag);

		tagsset = true;
		handler.OnRemoteTag(std::move(tag));
	}

public:
	void Start() noexcept override
	{
		MusicBrainzCDTagCache::getInstance()->requestTags(uri, device, this);
	}

	bool DisableTagCaching () override
	{
		return true;
	}
};
