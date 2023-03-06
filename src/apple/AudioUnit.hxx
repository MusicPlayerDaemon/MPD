// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef APPLE_AUDIO_UNIT_HXX
#define APPLE_AUDIO_UNIT_HXX

#include "Throw.hxx"

#include <AudioUnit/AudioUnit.h>

template<typename T>
T
AudioUnitGetPropertyT(AudioUnit inUnit, AudioUnitPropertyID inID,
		      AudioUnitScope inScope,
		      AudioUnitElement inElement)
{
	UInt32 size = sizeof(T);
	T value;

	OSStatus status = AudioUnitGetProperty(inUnit, inID, inScope,
					       inElement,
					       &value, &size);
	if (status != noErr)
		Apple::ThrowOSStatus(status);

	return value;
}

template<typename T>
void
AudioUnitSetPropertyT(AudioUnit inUnit, AudioUnitPropertyID inID,
		      AudioUnitScope inScope,
		      AudioUnitElement inElement,
		      const T &value)
{
	OSStatus status = AudioUnitSetProperty(inUnit, inID, inScope,
					       inElement,
					       &value, sizeof(value));
	if (status != noErr)
		Apple::ThrowOSStatus(status);
}

inline void
AudioUnitSetCurrentDevice(AudioUnit inUnit, const AudioDeviceID &value)
{
	AudioUnitSetPropertyT(inUnit, kAudioOutputUnitProperty_CurrentDevice,
			      kAudioUnitScope_Global, 0,
			      value);
}

inline void
AudioUnitSetInputStreamFormat(AudioUnit inUnit,
			      const AudioStreamBasicDescription &value)
{
	AudioUnitSetPropertyT(inUnit, kAudioUnitProperty_StreamFormat,
			      kAudioUnitScope_Input, 0,
			      value);
}

inline void
AudioUnitSetInputRenderCallback(AudioUnit inUnit,
				const AURenderCallbackStruct &value)
{
	AudioUnitSetPropertyT(inUnit, kAudioUnitProperty_SetRenderCallback,
			      kAudioUnitScope_Input, 0,
			      value);
}

inline UInt32
AudioUnitGetBufferFrameSize(AudioUnit inUnit)
{
	return AudioUnitGetPropertyT<UInt32>(inUnit,
					     kAudioDevicePropertyBufferFrameSize,
					     kAudioUnitScope_Global, 0);
}

inline void
AudioUnitSetBufferFrameSize(AudioUnit inUnit, const UInt32 &value)
{
	AudioUnitSetPropertyT(inUnit, kAudioDevicePropertyBufferFrameSize,
			      kAudioUnitScope_Global, 0,
			      value);
}

#endif
