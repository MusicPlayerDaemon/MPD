/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#ifndef MPD_CA_STREAM_HXX
#define MPD_CA_STREAM_HXX

#include <CoreAudio/CoreAudio.h>

#include <vector>

typedef std::vector<AudioStreamID> AudioStreamIdList;
typedef std::vector<AudioStreamRangedDescription> StreamFormatList;

class CoreAudioStream
{
public:
	CoreAudioStream();
	virtual ~CoreAudioStream();

	bool Open(AudioStreamID id);
	void Close(bool restore = true);

	AudioStreamID GetId() {return stream_id;}
	bool GetVirtualFormat(AudioStreamBasicDescription *desc);
	bool GetPhysicalFormat(AudioStreamBasicDescription *desc);
	bool SetVirtualFormat(AudioStreamBasicDescription *desc);
	bool SetPhysicalFormat(AudioStreamBasicDescription *desc);
	bool GetAvailablePhysicalFormats(StreamFormatList *stream_fmt_list);
	static bool GetAvailablePhysicalFormats(AudioStreamID id, StreamFormatList *stream_fmt_list);
	static bool GetStartingChannelInDevice(AudioStreamID id, UInt32 &starting_channel);

private:
	static OSStatus HardwareStreamListener(AudioObjectID inObjectID,
										   UInt32 inNumberAddresses, const AudioObjectPropertyAddress
										   inAddresses[], void* inClientData);
	AudioStreamID stream_id;
	AudioStreamBasicDescription original_virtual_fmt;
	AudioStreamBasicDescription original_physical_fmt;
};

#endif
