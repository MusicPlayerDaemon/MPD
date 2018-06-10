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

#include "CoreAudioHelpers.hxx"
#include "system/ByteOrder.hxx"
#include <sstream>
#include <vector>
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"

static constexpr Domain macos_output_domain("macos_output");

// Helper Functions
std::string
GetError(OSStatus error)
{
	char buffer[128];
 
	*(UInt32 *)(buffer + 1) = CFSwapInt32HostToBig(error);
	if (isprint(buffer[1]) && isprint(buffer[2]) && isprint(buffer[3]) && isprint(buffer[4]))
	{
		buffer[0] = buffer[5] = '\'';
		buffer[6] = '\0';
	}
	else
	{
		// no, format it as an integer
		sprintf(buffer, "%d", (int)error);
	}
	return std::string(buffer);
}

const char *
StreamDescriptionToString(const AudioStreamBasicDescription desc, std::string &str)
{
	char fourCC[5] = {
		(char)((desc.mFormatID >> 24) & 0xFF),
		(char)((desc.mFormatID >> 16) & 0xFF),
		(char)((desc.mFormatID >>  8) & 0xFF),
		(char) (desc.mFormatID        & 0xFF),
		0
	};

	std::stringstream sstr;
	switch (desc.mFormatID)
	{
		case kAudioFormatLinearPCM:
			sstr  << "["
				<< fourCC
				<< "] "
				<< ((desc.mFormatFlags & kAudioFormatFlagIsNonMixable) ? "" : "Mixable " )
				<< ((desc.mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? "Non-" : "" )
				<< "Interleaved "
				<< desc.mChannelsPerFrame
				<< " Channel "
				<< desc.mBitsPerChannel
				<< "-bit "
				<< ((desc.mFormatFlags & kAudioFormatFlagIsFloat) ? "Floating Point " : "Signed Integer ")
				<< ((desc.mFormatFlags & kAudioFormatFlagIsBigEndian) ? "BE" : "LE")
				<< " ("
				<< (UInt32)desc.mSampleRate
				<< "Hz)";
			str = sstr.str();
			break;
		case kAudioFormatAC3:
			sstr  << "["
				<< fourCC
				<< "] "
				<< ((desc.mFormatFlags & kAudioFormatFlagIsBigEndian) ? "BE" : "LE")
				<< " AC-3/DTS ("
				<< (UInt32)desc.mSampleRate
				<< "Hz)";
			str = sstr.str();
			break;
		case kAudioFormat60958AC3:
			sstr  << "["
				<< fourCC
				<< "] AC-3/DTS for S/PDIF "
				<< ((desc.mFormatFlags & kAudioFormatFlagIsBigEndian) ? "BE" : "LE")
				<< " ("
				<< (UInt32)desc.mSampleRate
				<< "Hz)";
			str = sstr.str();
			break;
		default:
			sstr  << "["
				<< fourCC
				<< "]";
			break;
	}
	return str.c_str();
}

AudioBufferList *
AllocateABL(const AudioStreamBasicDescription asbd, const UInt32 capacity_frames)
{
	AudioBufferList *buffer_list = nullptr;
	UInt32 num_buffers = (asbd.mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? asbd.mChannelsPerFrame : 1;
	
	if((buffer_list = static_cast<AudioBufferList *>(calloc(1, offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * num_buffers)))) == nullptr)
		throw std::runtime_error("Unable to allocate memory for AudioBufferList.");
	else
	{
		buffer_list->mNumberBuffers = num_buffers;
		for(UInt32 buffer_index = 0; buffer_index < buffer_list->mNumberBuffers; ++buffer_index)
		{
			try
			{
				AllocateAudioBuffer(buffer_list->mBuffers[buffer_index], asbd, capacity_frames);
			}
			catch (...)
			{
				DeallocateABL(buffer_list);
				std::throw_with_nested("Unable to allocate memory for AudioBufferList.");
			}
		}
	}
	return buffer_list;
}

void
DeallocateABL(AudioBufferList *buffer_list)
{
	if(buffer_list != nullptr)
	{
		for(UInt32 buffer_index = 0; buffer_index < buffer_list->mNumberBuffers; ++buffer_index) {
			free(buffer_list->mBuffers[buffer_index].mData);
		}
	}
	free(buffer_list);
}

void
AllocateAudioBuffer(AudioBuffer &buffer, const AudioStreamBasicDescription asbd, const UInt32 capacity_frames)
{
	if((buffer.mData = static_cast<void *>(calloc(capacity_frames, asbd.mBytesPerFrame))) == nullptr)
	   throw std::runtime_error("Unable to allocate memory for AudioBuffer.");
	buffer.mDataByteSize = capacity_frames * asbd.mBytesPerFrame;
	buffer.mNumberChannels = (asbd.mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? 1 : asbd.mChannelsPerFrame;
}

AudioStreamBasicDescription
AudioFormatToASBD(AudioFormat format)
{
	assert(format.format != SampleFormat::UNDEFINED);
	assert(format.format != SampleFormat::DSD);
	
	AudioStreamBasicDescription out_format;
	memset(&out_format, 0, sizeof(out_format));
	out_format.mSampleRate = format.sample_rate;
	out_format.mChannelsPerFrame = format.channels;
	out_format.mFormatID = kAudioFormatLinearPCM;
	out_format.mFramesPerPacket = 1;
	out_format.mBytesPerPacket = out_format.mBytesPerFrame = format.GetFrameSize();
	
	switch (format.format)
	{
		case SampleFormat::S8:
			out_format.mBitsPerChannel = 8;
			out_format.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			break;
		case SampleFormat::S16:
			out_format.mBitsPerChannel = 16;
			out_format.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			break;
		case SampleFormat::S24_P32:
			out_format.mBitsPerChannel = 24;
			out_format.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			break;
		case SampleFormat::S32:
			out_format.mBitsPerChannel = 32;
			out_format.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			break;
		case SampleFormat::FLOAT:
			out_format.mBitsPerChannel = 32;
			out_format.mFormatFlags = kAudioFormatFlagIsFloat;
			break;
		default:
			break;
	}
	if(IsBigEndian())
		out_format.mFormatFlags |= kAudioFormatFlagIsBigEndian;
	return out_format;
}

AudioFormat
ASBDToAudioFormat(AudioStreamBasicDescription asbd)
{
	assert(asbd.mFormatID == kAudioFormatLinearPCM);

	AudioFormat out_format;
	out_format.sample_rate = asbd.mSampleRate;
	out_format.channels = asbd.mChannelsPerFrame;;
	
	if (asbd.mFormatFlags & kAudioFormatFlagIsFloat)
		out_format.format = SampleFormat::FLOAT;
	else
	{
		switch (asbd.mBitsPerChannel) {
			case 8:
				out_format.format = SampleFormat::S8;
				break;
			case 16:
				out_format.format = SampleFormat::S16;
				break;
			case 24:
				out_format.format = SampleFormat::S24_P32;
				break;
			case 32:
				out_format.format = SampleFormat::S32;
				break;
		}
	}
	return out_format;
}

void
ParseChannelMap(const char *channel_map_str, std::vector<SInt32> &channel_map)
{
	char *endptr;
	bool want_number = true;

	while (*channel_map_str)
	{
		if (!want_number && *channel_map_str == ',')
		{
			++channel_map_str;
			want_number = true;
			continue;
		}

		if (want_number && (isdigit(*channel_map_str) || *channel_map_str == '-'))
		{
			channel_map.push_back((SInt32)strtol(channel_map_str, &endptr, 10));
			if (channel_map.back() < -1)
				throw FormatRuntimeError("Channel map value %d not allowed (must be -1 or greater)", channel_map.back());
			channel_map_str = endptr;
			want_number = false;
			FormatDebug(macos_output_domain, "channel_map[%u] = %d", (UInt32)channel_map.size() - 1, channel_map.back());
			continue;
		}
		throw FormatRuntimeError("Invalid character '%c' in channel map", *channel_map_str);
	}
}
