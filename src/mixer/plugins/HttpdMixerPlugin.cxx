/*
 * Copyright 2003-2019 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "mixer/MixerInternal.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "config/Block.hxx"

#include <noson/sonossystem.h>

static constexpr Domain httpd_mixer_domain("httpd_mixer");

SONOS::System * sonos = 0;

void handleEventCB(void* handle)
{
        unsigned char mask = sonos->LastEvents();
        if ((mask & SONOS::SVCEvent_ZGTopologyChanged))
        {
                LogDebug(httpd_mixer_domain,
                        "Noson ZGTopologyChanged event triggered");
        }
}

class HttpdMixer final : public Mixer {
	/**
	 * The current volume in percent (0..100).
	 */
	unsigned volume = 0;
	std::string tryzone;
	// MPDs httpd output as a local radio station
	std::string stream = "x-rincon-mp3radio://";
	bool online = false;
//	uint8_t loudness;
	virtual ~HttpdMixer();

public:
	HttpdMixer(MixerListener &_listener, const ConfigBlock &block)
		:Mixer(httpd_mixer_plugin, _listener)
	{
		Configure(block);
	}

	void Configure(const ConfigBlock &block);

	/* virtual methods from class Mixer */
	void Open() override;

	void Close() noexcept override {
	}

	int GetVolume() override;
	void SetVolume(unsigned _volume) override;
};

void HttpdMixer::Configure(const ConfigBlock &block)
{
	tryzone = block.GetBlockValue("mixer_zone", "");
	if (block.GetBlockValue("mixer_stream"))
	{
		stream.append(block.GetBlockValue("mixer_stream"));
	}
	else
	{
		LogWarning(httpd_mixer_domain, "Nonson empty stream URL");
	}
}

void HttpdMixer::Open()
{
	FormatDebug(httpd_mixer_domain, "Noson libnoson %s",
		SONOS::libVersionString());
	sonos = new SONOS::System(0, handleEventCB);
	if (sonos->Discover())
	{
		LogInfo(httpd_mixer_domain,
			"Noson discovered compatible device(s)");

		/* Players */
		SONOS::ZonePlayerList players = sonos->GetZonePlayerList();
		for (SONOS::ZonePlayerList::const_iterator it =
			players.begin(); it != players.end(); ++it)
		{
			FormatDebug(httpd_mixer_domain,
				"Noson found player '%s' with UUID '%s'",
				it->first.c_str(),
				it->second->GetUUID().c_str());
		}

		/* Zones */
		SONOS::ZoneList zones = sonos->GetZoneList();
		for (SONOS::ZoneList::const_iterator it =
			zones.begin(); it != zones.end(); ++it)
		{
			FormatDebug(httpd_mixer_domain,
				"Noson found zone '%s' with coordinator '%s'",
				it->second->GetZoneName().c_str(),
				it->second->GetCoordinator()->c_str());
		}

		/* Connect */
		for (SONOS::ZoneList::const_iterator it =
                        zones.begin(); it != zones.end(); ++it)
                {
			if (tryzone.empty() || it->second->GetZoneName() ==
				tryzone)
			{
				if (sonos->ConnectZone(it->second, 0, handleEventCB))
				{
					FormatDebug(httpd_mixer_domain,
						"Noson connected to zone '%s'",
						sonos->GetConnectedZone()->GetZoneName().c_str());
					break;
				}
			}
		}

		/* Play URL */
		if (sonos->GetPlayer()->PlayStream(stream, "MusicPlayerDaemon"))
		{
			FormatDebug(httpd_mixer_domain,
				"Noson playing url '%s'", stream.c_str());
			online = true;
		}
		else
		{
			FormatWarning(httpd_mixer_domain,
				"Noson failed to play url '%s'", stream.c_str());
			online = false;
		}
	}
}

int HttpdMixer::GetVolume()
{
	uint8_t _volume = 0;
	if (online == false)
	{
		volume = _volume;
		return volume;
	}

	SONOS::ZonePtr pl = sonos->GetConnectedZone();
	for (SONOS::Zone::iterator ip = pl->begin();
		ip != pl->end(); ++ip)
	{
		if (sonos->GetPlayer()->GetVolume((*ip)->GetUUID(), &_volume))
		{
			FormatDebug(httpd_mixer_domain,
				"Noson retrieved volume level '%d'", _volume);
		}
		else
		{
			FormatWarning(httpd_mixer_domain,
				"Noson failed to retrieve volume");
		}
	}
	volume = _volume;
	return volume;
}

void HttpdMixer::SetVolume(unsigned _volume)
{
	if (online == false)
	{
		return;
	}
	SONOS::ZonePtr pl = sonos->GetConnectedZone();
	for (SONOS::Zone::iterator ip = pl->begin();
		ip != pl->end(); ++ip)
	{
		if (sonos->GetPlayer()->SetVolume((*ip)->GetUUID(), _volume))
		{
			FormatDebug(httpd_mixer_domain,
				"Noson changed volume to level '%d'", _volume);
		}
		else
		{
			FormatWarning(httpd_mixer_domain,
				"Noson failed to change volume");
		}
	}
}

HttpdMixer::~HttpdMixer()
{
	delete sonos;
	sonos = 0;

}
static Mixer *
httpd_mixer_init(gcc_unused EventLoop &event_loop,
		gcc_unused AudioOutput &ao,
		MixerListener &listener,
		const ConfigBlock &block)
{
	return new HttpdMixer(listener, block);
}

const MixerPlugin httpd_mixer_plugin = {
	httpd_mixer_init,
	true,
};
