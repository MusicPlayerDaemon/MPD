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

#ifndef MPD_CA_DEVICE_HXX
#define MPD_CA_DEVICE_HXX

#include <string>
#include <vector>
#include "CoreAudioStream.hxx"
#include "AudioFormat.hxx"
#include <CoreAudio/CoreAudio.h>

class CoreAudioDevice
{
public:
	CoreAudioDevice();
	explicit CoreAudioDevice(AudioDeviceID dev_id);
	virtual ~CoreAudioDevice();
	// Initialize and search for device name, falls back to system default device
	bool Open(const std::string &device_name);
	// Restore settings and close the device
	void Close();
	// Starts the device (i.e. enables CoreAudio HAL to callback for data)
	void Start();
	// Stops the device (used for pause, no data will be requested)
	void Stop();
	AudioDeviceID GetId() {return device_id;}
	std::string GetName() const;
	bool IsPlanar() {return is_planar;}
	UInt32 GetTotalOutputChannels() const;
	UInt32 GetNumChannelsOfStream(UInt32 stream_idx) const;
	UInt32 GetStreamIdx() {return output_stream_idx;}
	bool GetStreams(AudioStreamIdList *stream_id_list);
	// Turn on exclusive device usage
	bool SetHogStatus(bool hog);
	pid_t GetHogStatus();
	bool HasVolume() {return has_volume;}
	bool SetCurrentVolume(Float32 vol);
	Float32 GetCurrentVolume();
	UInt32 GetBufferSize();
	bool SetBufferSize(UInt32 size);
	bool AddIOProc(AudioDeviceIOProc io_proc, void* callback_data);
	bool RemoveIOProc();
	/** Main method called for every MPD format change.
	 *	Uses scoring to find the best matching output
	 *	format for MPD AudioFormat format. In case
	 *	prefer_unmixable is set, unmixable integer
	 *	formats are used.
	 */
	bool SetFormat(const AudioFormat &format, bool prefer_unmixable);
	// Return physical format used internally by the device output stream
	AudioStreamBasicDescription GetPhysFormat();
	// Return the IO format that the device expects to be fed
	AudioStreamBasicDescription GetIOFormat();
	
private:

	bool started;
	// To identify devices where streams are single channels
	bool is_planar;
	AudioDeviceID device_id;
	UInt32 output_stream_idx;
	AudioStreamBasicDescription output_format;
	AudioDeviceIOProc io_proc;
	CoreAudioStream output_stream;
	bool has_volume;
	pid_t hog_pid;
	UInt32 buffer_size_restore;
	
	// Collect information from the device
	bool Enumerate();
	
	
	/** Scores a format based on:
	 *	1. Matching sample rate (or integer fraction / multiple)
	 *	2. Matching bits per channel (or higher).
	 *	3. Matching number of channels (or higher).
	 *
	 *	The ASBD which should be scored against the
	 *	MPD AudioFormat format which should be matched
	 *	to be passed.
	 */
	float ScoreFormat(const AudioStreamBasicDescription &format_desc, const AudioFormat &format) const;
	
	/** Scores a samplerate based on:
	 * 	1. Prefer exact match
	 *	2. Prefer exact multiple of source samplerate
	 *	The destination samplerate to score is compared against
	 *	the requested source rate of the audio format.
	 */
	float ScoreSampleRate(Float64 destination_rate, unsigned int source_rate) const;
	
	typedef struct
	{
		AudioStreamID stream_id;
		StreamFormatList format_list;
		UInt32 num_channels;
	} stream_info;
	
	std::vector<stream_info>       stream_infos;
};

#endif
