/*
 * Copyright 2020 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
