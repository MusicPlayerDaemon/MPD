// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_WASAPI_DEVICE_NOTIFICATION_HXX
#define MPD_WASAPI_DEVICE_NOTIFICATION_HXX

#include <mmdeviceapi.h>

#include <synchapi.h>

/**
 * IMMNotificationClient implementation that detects audio
 * device changes on Windows.
 *
 * https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nn-mmdeviceapi-immnotificationclient
 * https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immnotificationclient-ondefaultdevicechanged
 */

/**
 * FIXME: assuming two wasapi outputs - A and B - are
 * configured with explicit "device" fields set:
 * When playback is active on output A and a disconnected
 * device reconnects to output B, output B will not resume
 * until REOPEN_AFTER has elapsed since the disconnect,
 * or the user manually triggers a pause/play cycle.
 */
class WasapiDeviceNotification final : public IMMNotificationClient {
	HANDLE device_event;
	LONG ref_count = 1;

	/**
	 * When true, #OnDeviceStateChanged will signal the
	 * device_event.
	 *
	 * For default device configurations,
	 * this is suppressed because OnDeviceStateChanged
	 * may fire before OnDefaultDeviceChanged, causing
	 * the output to close prematurely.
	 *
	 * For explicit device configurations, this allows
	 * detecting device reconnects.
	 */
	const bool watch_state;

public:
	WasapiDeviceNotification(HANDLE _device_event,
				bool _watch_state) noexcept
		:device_event(_device_event),
		 watch_state(_watch_state) {}

	/* IUnknown */
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
					      void **ppv) noexcept override {
		if (riid == IID_IUnknown ||
		    riid == __uuidof(IMMNotificationClient)) {
			*ppv = static_cast<IMMNotificationClient *>(this);
			AddRef();
			return S_OK;
		}
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() noexcept override {
		return InterlockedIncrement(&ref_count);
	}

	ULONG STDMETHODCALLTYPE Release() noexcept override {
		ULONG count = InterlockedDecrement(&ref_count);
		if (count == 0)
			delete this;
		return count;
	}

	/* IMMNotificationClient */
	HRESULT STDMETHODCALLTYPE
	OnDeviceStateChanged(LPCWSTR, DWORD) noexcept override {
		if (watch_state)
			SetEvent(device_event);
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE
	OnDeviceAdded(LPCWSTR) noexcept override {
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE
	OnDeviceRemoved(LPCWSTR) noexcept override {
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE
	OnDefaultDeviceChanged(EDataFlow flow, ERole role,
			      LPCWSTR) noexcept override {
		if (flow == eRender && role == eMultimedia)
			SetEvent(device_event);
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE
	OnPropertyValueChanged(LPCWSTR,
			      const PROPERTYKEY) noexcept override {
		return S_OK;
	}
};

#endif
