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

/*   Build MPD with -Dnoson=enabled option
 *   Noson 2.0.0 (or better) C++ library
 *   $ git clone https://github.com/janbar/noson.git
 *   $ cd noson
 *   $ mkdir build
 *   $ cmake -DCMAKE_BUILD_TYPE=Release -DHAVE_FLAC=0 -DHAVE_PULSEAUDIO=0 ..
 *   $ make
  *  # make install
*/

#include "mixer/MixerInternal.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "config/Block.hxx"

#include <noson/sonossystem.h>

static constexpr Domain httpd_mixer_domain("httpd_mixer");

SONOS::System * gSonos = 0;
SONOS::PlayerPtr gPlayer;

void handleEventCB(void* handle)
{
        unsigned char mask = gSonos->LastEvents();
	if ((mask & SONOS::SVCEvent_AlarmClockChanged))
        {
                LogDebug(httpd_mixer_domain,
                        "Noson AlarmClockChanged event triggered");
        }
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
	std::string zone = "";
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
	zone = block.GetBlockValue("mixer_zone", "");
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

	gSonos = new SONOS::System(0, handleEventCB);

	FormatInfo(httpd_mixer_domain,
		"Noson searching for zone '%s'", zone.c_str());
	if (gSonos->Discover())
	{
		LogInfo(httpd_mixer_domain,
			"Noson zone(s) discovered");
	}
	else
	{
		LogInfo(httpd_mixer_domain,
			"Noson no zone available");
		return;
        }


	/* Players debug only */
	SONOS::ZonePlayerList players = gSonos->GetZonePlayerList();
	for (SONOS::ZonePlayerList::const_iterator it = players.begin();
		it != players.end(); ++it)
	{
		FormatDebug(httpd_mixer_domain,
			"Noson found player '%s' with UUID '%s'",
			it->first.c_str(),
			it->second->GetUUID().c_str());
	}

	/* Zones debug only */
	SONOS::ZoneList zones = gSonos->GetZoneList();
	for (SONOS::ZoneList::const_iterator it =
		zones.begin(); it != zones.end(); ++it)
	{
		FormatDebug(httpd_mixer_domain,
			"Noson found zone '%s' with coordinator '%s'",
			it->second->GetZoneName().c_str(),
			it->second->GetCoordinator()->c_str());
	}

	/* Connect */
	zones = gSonos->GetZoneList();

	bool found = false;
	for (SONOS::ZoneList::const_iterator iz = zones.begin();
		iz != zones.end(); ++iz)
	{
		if (iz->second->GetZoneName() == zone || zone.empty())
		{
			found = true;
			if ((gPlayer = gSonos->GetPlayer(iz->second, 0, 0)))
			{
				FormatInfo(httpd_mixer_domain,
					"Noson connected to zone '%s'",
					zone.c_str());
				break;
			}
			else
			{
				FormatInfo(httpd_mixer_domain,
					"Noson failed connecting to zone '%s'",
					zone.c_str());
				return;
			}
			if (!found)
			{
				FormatInfo(httpd_mixer_domain,
					"Noson did not discover zone '%s'",
					zone.c_str());
				return;

			}
		}

	}

	/* Play URL */
	if (gPlayer->PlayStream(stream, "MusicPlayerDaemon"))
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

int HttpdMixer::GetVolume()
{
	uint8_t _volume = 0;
	if (online == false)
	{
		volume = _volume;
		return volume;
	}

	SONOS::ZonePtr pl = gPlayer->GetZone();
	for (SONOS::Zone::iterator ip = pl->begin();
		ip != pl->end(); ++ip)
	{
		if (gPlayer->GetVolume((*ip)->GetUUID(), &_volume))
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
	SONOS::ZonePtr pl = gPlayer->GetZone();
	for (SONOS::Zone::iterator ip = pl->begin();
		ip != pl->end(); ++ip)
	{
		if (gPlayer->SetVolume((*ip)->GetUUID(), _volume))
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
	if (gPlayer)
	{
		gPlayer.reset();
	}
	delete gSonos;
	gSonos = 0;

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
