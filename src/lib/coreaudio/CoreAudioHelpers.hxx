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

#include <string>
#include "AudioFormat.hxx"
#include "util/StringBuffer.hxx"
#include <CoreAudio/CoreAudio.h>

// Helper Functions
// Return ID of device with search_name
AudioDeviceID FindAudioDevice(const char *search_name);
// Return ID of the default audio device
AudioDeviceID GetDefaultOutputDevice();
// Get a human readable error description for OSStatus
const char* GetError(OSStatus error);
// Transform ASBD to readable string for printout
StringBuffer<64> StreamDescriptionToString(const AudioStreamBasicDescription desc);
// Allocate an AudioBufferList struct with capacity
AudioBufferList* AllocateABL(AudioStreamBasicDescription asbd, UInt32 capacity_frames);
// Frees the memory pointed to by the AudioBufferList pointer
void DeallocateABL(AudioBufferList *buffer_list);
// Allocate memory for an AudioBuffer with capactiy
void AllocateAudioBuffer(AudioBuffer &buffer, AudioStreamBasicDescription asbd, UInt32 capacity_frames);
// Get ASBD derived from MPD Audioformat
AudioStreamBasicDescription AudioFormatToASBD(AudioFormat format);
// Get MPD Audioformat from CoreAudio ASBD
AudioFormat ASBDToAudioFormat(AudioStreamBasicDescription asbd);
// Parse channel map from string
void ParseChannelMap(const char* channel_map_str, std::vector<SInt32> &channel_map);
