// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_WASAPI_DEVICE_COLLECTION_HXX
#define MPD_WASAPI_DEVICE_COLLECTION_HXX

#include "win32/ComPtr.hxx"
#include "win32/HResult.hxx"

#include <mmdeviceapi.h>

inline ComPtr<IMMDevice>
GetDefaultAudioEndpoint(IMMDeviceEnumerator &e)
{
	IMMDevice *device = nullptr;

	HRESULT result = e.GetDefaultAudioEndpoint(eRender, eMultimedia,
						   &device);
	if (FAILED(result))
		throw MakeHResultError(result,
				       "Unable to get default device for multimedia");

	return ComPtr{device};
}

inline ComPtr<IMMDeviceCollection>
EnumAudioEndpoints(IMMDeviceEnumerator &e)
{
	IMMDeviceCollection *dc = nullptr;

	HRESULT result = e.EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
					      &dc);
	if (FAILED(result))
		throw MakeHResultError(result, "Unable to enumerate devices");

	return ComPtr{dc};
}

inline UINT
GetCount(IMMDeviceCollection &dc)
{
	UINT count;

	HRESULT result = dc.GetCount(&count);
	if (FAILED(result))
		throw MakeHResultError(result, "Collection->GetCount failed");

	return count;
}

inline ComPtr<IMMDevice>
Item(IMMDeviceCollection &dc, UINT i)
{
	IMMDevice *device = nullptr;

	auto result = dc.Item(i, &device);
	if (FAILED(result))
		throw MakeHResultError(result, "Collection->Item failed");

	return ComPtr{device};
}

inline DWORD
GetState(IMMDevice &device)
{
	DWORD state;

	HRESULT result = device.GetState(&state);;
	if (FAILED(result))
		throw MakeHResultError(result, "Unable to get device status");

	return state;
}

template<typename T>
inline ComPtr<T>
Activate(IMMDevice &device)
{
	T *p = nullptr;
	HRESULT result = device.Activate(__uuidof(T), CLSCTX_ALL,
					 nullptr, (void **)&p);
	if (FAILED(result))
		throw MakeHResultError(result, "Unable to activate device");

	return ComPtr{p};
}

inline ComPtr<IPropertyStore>
OpenPropertyStore(IMMDevice &device)
{
	IPropertyStore *property_store = nullptr;

	HRESULT result = device.OpenPropertyStore(STGM_READ, &property_store);
	if (FAILED(result))
		throw MakeHResultError(result,
				       "Device->OpenPropertyStore failed");

	return ComPtr{property_store};
}

#endif
