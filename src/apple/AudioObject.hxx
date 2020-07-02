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

#ifndef APPLE_AUDIO_OBJECT_HXX
#define APPLE_AUDIO_OBJECT_HXX

#include "Throw.hxx"
#include "util/AllocatedArray.hxx"

#include <CoreAudio/AudioHardware.h>

#include <cstddef>

namespace Apple {
class StringRef;
}

inline std::size_t
AudioObjectGetPropertyDataSize(AudioObjectID inObjectID,
			       const AudioObjectPropertyAddress &inAddress)
{
	UInt32 size;
	OSStatus status = AudioObjectGetPropertyDataSize(inObjectID,
							 &inAddress,
							 0, nullptr, &size);
	if (status != noErr)
		Apple::ThrowOSStatus(status);

	return size;
}

template<typename T>
T
AudioObjectGetPropertyDataT(AudioObjectID inObjectID,
			    const AudioObjectPropertyAddress &inAddress)
{
	OSStatus status;
	UInt32 size = sizeof(T);
	T value;

	status = AudioObjectGetPropertyData(inObjectID, &inAddress,
					    0, nullptr,
					    &size, &value);
	if (status != noErr)
		Apple::ThrowOSStatus(status);

	return value;
}

Apple::StringRef
AudioObjectGetStringProperty(AudioObjectID inObjectID,
			     const AudioObjectPropertyAddress &inAddress);

template<typename T>
AllocatedArray<T>
AudioObjectGetPropertyDataArray(AudioObjectID inObjectID,
			       const AudioObjectPropertyAddress &inAddress)
{
	OSStatus status;
	UInt32 size;

	status = AudioObjectGetPropertyDataSize(inObjectID,
						&inAddress,
						0, nullptr, &size);
	if (status != noErr)
		Apple::ThrowOSStatus(status);

	AllocatedArray<T> result(size / sizeof(T));

	status = AudioObjectGetPropertyData(inObjectID, &inAddress,
					    0, nullptr,
					    &size, result.begin());
	if (status != noErr)
		Apple::ThrowOSStatus(status);

	return result;
}

#endif
