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

#include <vector>
#include "CoreAudioStream.hxx"
#include "AudioFormat.hxx"
#include <CoreAudio/CoreAudio.h>

class CoreAudioDevice
{
public:
	CoreAudioDevice() noexcept;
	explicit CoreAudioDevice(AudioDeviceID dev_id) noexcept;
	~CoreAudioDevice();
	/** Initialize and search for device with device_name.
	 *	Opens CoreAudio default output device in case
	 *	"default" is passed. Throws runtime exception in
	 *	case the device cannot be opened.
	 */
	void Open(const char *device_name);
	// Restore settings and close the device
	void Close() noexcept;
	// Starts the device (i.e. enables CoreAudio HAL to callback for data)
	void Start();
	// Stops the device (used for pause, no data will be requested)
	void Stop();
	AudioDeviceID GetId() {return device_id;}
	const char * GetName();
	bool IsPlanar() {return is_planar;}
	UInt32 GetStreamIdx() {return output_stream_idx;}
	AudioStreamIdList GetStreams();
	// Toggle for exclusive device usage
	void SetHogStatus(bool hog);
	pid_t GetHogStatus();
	bool HasVolume() {return has_volume;}
	/** Set the volume for float in range
	 *	[0, 1]. No-op in case no volume
	 *	setting is supported by device.
	 *	Throws runtime exception in case
	 *	volume could not be set successfully.
	 */
	void SetCurrentVolume(Float32 vol);
	/** Get current volume ranged [0, 1]
	 *	of the device. Returns -1 on
	 *	error or in case the device
	 *	does not support volume
	 *	setting.
	 */
	Float32 GetCurrentVolume();
	UInt32 GetBufferSize();
	void SetBufferSize(UInt32 size);
	void AddIOProc(AudioDeviceIOProc io_proc, void* callback_data);
	void RemoveIOProc();
	/** Main method called for every MPD format change.
	 *	Uses scoring to find the best matching output
	 *	format for MPD AudioFormat. In case prefer_unmixable
	 *	is set, unmixable integer formats are used (if
	 *	supported). Returns true if some matching
	 *	format is found.
	 */
	bool SetFormat(const AudioFormat &format, bool prefer_unmixable);
	// Return physical format used internally by the device output stream
	AudioStreamBasicDescription GetPhysFormat();
	// Return the IO format that CoreAudio expects to be fed in the callback
	AudioStreamBasicDescription GetIOFormat();
	
private:
	bool started = false;
	// To identify devices where streams are single channels
	bool is_planar = true;
	AudioDeviceID device_id = 0;
	UInt32 output_stream_idx = 0;
	AudioDeviceIOProc io_proc = nullptr;
	char *dev_name = nullptr;
	bool has_volume = false;
	pid_t hog_pid = -1;
	CoreAudioStream output_stream;
	AudioStreamBasicDescription output_format;
	/** This gets assigned when first changing the
	 *	device frame buffer size by calling
	 *	SetBufferSize. Used to restore buffer size
	 *	on Close.
	 */
	UInt32 buffer_size_restore = 0;
	
	UInt32 GetTotalOutputChannels() const;
	UInt32 GetNumChannelsOfStream(UInt32 stream_idx) const;
	
	/** This method enumerates the device by collecting
	 *	information from the device. After successful
	 *	enumeration stream_infos vector holds all
	 *	CoreAudioStreams and related format information
	 *	which is required to use SetFormat method.
	 */
	void Enumerate();
	
	/** Scores a format based on:
	 *	1. Matching sample rate (or integer fraction / multiple)
	 *	2. Matching bits per channel (or higher).
	 *	3. Matching number of channels (or higher).
	 *
	 *	The ASBD which should be scored against the
	 *	MPD AudioFormat format which should be matched
	 *	to be passed. The return value correponds to the
	 *	score of the ASBD as match of the AudioFormat
	 *	(higher is better).
	 */
	float ScoreFormat(const AudioStreamBasicDescription &format_desc, const AudioFormat &format) const;
	
	/** Scores a samplerate based on:
	 * 	1. Prefer exact match
	 *	2. Prefer exact multiple of source samplerate
	 *	The destination samplerate to score is compared against
	 *	the requested source rate of the audio format. Returns
	 *	float value that corresponds to the score of
	 *	destination_rate as match of the source_rate.
	 */
	float ScoreSampleRate(Float64 destination_rate, unsigned int source_rate) const;
	
	struct StreamInfo {
		AudioStreamID stream_id;
		StreamFormatList format_list;
		UInt32 num_channels;
	};
	
	std::vector<StreamInfo> stream_infos;
};

#endif
