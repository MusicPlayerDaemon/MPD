// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_WASAPI_AUDIO_CLIENT_HXX
#define MPD_WASAPI_AUDIO_CLIENT_HXX

#include "win32/ComHeapPtr.hxx"
#include "win32/ComPtr.hxx"
#include "win32/HResult.hxx"

#include <audioclient.h>

inline UINT32
GetBufferSizeInFrames(IAudioClient &client)
{
	UINT32 buffer_size_in_frames;

	HRESULT result = client.GetBufferSize(&buffer_size_in_frames);
	if (FAILED(result))
		throw MakeHResultError(result,
				       "Unable to get audio client buffer size");

	return buffer_size_in_frames;
}

inline UINT32
GetCurrentPaddingFrames(IAudioClient &client)
{
	UINT32 padding_frames;

	HRESULT result = client.GetCurrentPadding(&padding_frames);
	if (FAILED(result))
		throw MakeHResultError(result,
				       "Failed to get current padding");

	return padding_frames;
}

inline ComHeapPtr<WAVEFORMATEX>
GetMixFormat(IAudioClient &client)
{
	WAVEFORMATEX *f;

	HRESULT result = client.GetMixFormat(&f);
	if (FAILED(result))
		throw MakeHResultError(result, "GetMixFormat failed");

	return ComHeapPtr{f};
}

inline void
Start(IAudioClient &client)
{
	HRESULT result = client.Start();
	if (FAILED(result))
		throw MakeHResultError(result, "Failed to start client");
}

inline void
Stop(IAudioClient &client)
{
	HRESULT result = client.Stop();
	if (FAILED(result))
		throw MakeHResultError(result, "Failed to stop client");
}

inline void
SetEventHandle(IAudioClient &client, HANDLE h)
{
	HRESULT result = client.SetEventHandle(h);
	if (FAILED(result))
		throw MakeHResultError(result, "Unable to set event handle");
}

template<typename T>
inline ComPtr<T>
GetService(IAudioClient &client)
{
	T *p = nullptr;
	HRESULT result = client.GetService(IID_PPV_ARGS(&p));
	if (FAILED(result))
		throw MakeHResultError(result, "Unable to get service");

	return ComPtr{p};
}

#endif
