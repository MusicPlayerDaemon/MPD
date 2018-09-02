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

class CoreAudioStream {
public:
	CoreAudioStream();
	~CoreAudioStream();

	void Open(AudioStreamID id);
	void Close() noexcept;

	AudioStreamID GetId() {return stream_id;}
	AudioStreamBasicDescription GetVirtualFormat();
	AudioStreamBasicDescription GetPhysicalFormat();
	void SetVirtualFormat(AudioStreamBasicDescription desc);
	void SetPhysicalFormat(AudioStreamBasicDescription desc);
	StreamFormatList GetAvailablePhysicalFormats();
	static StreamFormatList GetAvailablePhysicalFormats(AudioStreamID id);

private:
	AudioStreamID stream_id = 0;
	AudioStreamBasicDescription original_virtual_fmt;
	AudioStreamBasicDescription original_physical_fmt;
};

#endif
