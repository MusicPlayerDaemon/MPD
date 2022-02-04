/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef _UPNPPDISC_H_X_INCLUDED_
#define _UPNPPDISC_H_X_INCLUDED_

#include "Compat.hxx"
#include "Callback.hxx"
#include "Device.hxx"
#include "lib/curl/Init.hxx"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Request.hxx"
#include "thread/Mutex.hxx"
#include "event/InjectEvent.hxx"
#include "util/IntrusiveList.hxx"

#include <list>
#include <vector>
#include <string>
#include <chrono>

class ContentDirectoryService;

class UPnPDiscoveryListener {
public:
	virtual void FoundUPnP(const ContentDirectoryService &service) = 0;
	virtual void LostUPnP(const ContentDirectoryService &service) = 0;
};

/**
 * Manage UPnP discovery and maintain a directory of active devices. Singleton.
 *
 * We are only interested in MediaServers with a ContentDirectory service
 * for now, but this could be made more general, by removing the filtering.
 */
class UPnPDeviceDirectory final : UpnpCallback {
	/**
	 * Descriptor for one device having a Content Directory
	 * service found on the network.
	 */
	class ContentDirectoryDescriptor {
	public:
		std::string id;

		UPnPDevice device;

		/**
		 * The time stamp when this device expires.
		 */
		std::chrono::steady_clock::time_point expires;

		ContentDirectoryDescriptor() = default;

		ContentDirectoryDescriptor(std::string &&_id,
					   std::chrono::steady_clock::time_point last,
					   std::chrono::steady_clock::duration exp) noexcept
			:id(std::move(_id)),
			 expires(last + exp + std::chrono::seconds(20)) {}

		void Parse(const std::string &url, const char *description) {
			device.Parse(url, description);
		}
	};

	class Downloader final
		: public IntrusiveListHook, CurlResponseHandler
	{
		InjectEvent defer_start_event;

		UPnPDeviceDirectory &parent;

		std::string id;
		const std::string url;
		const std::chrono::steady_clock::duration expires;

		CurlRequest request;

		std::string data;

	public:
		Downloader(UPnPDeviceDirectory &_parent,
			   const UpnpDiscovery &disco);

		void Start() noexcept {
			defer_start_event.Schedule();
		}

		void Destroy() noexcept;

	private:
		void OnDeferredStart() noexcept {
			try {
				request.Start();
			} catch (...) {
				OnError(std::current_exception());
			}
		}

		/* virtual methods from CurlResponseHandler */
		void OnHeaders(unsigned status, Curl::Headers &&headers) override;
		void OnData(ConstBuffer<void> data) override;
		void OnEnd() override;
		void OnError(std::exception_ptr e) noexcept override;
	};

	CurlInit curl;

	const UpnpClient_Handle handle;
	UPnPDiscoveryListener *const listener;

	Mutex mutex;

	/**
	 * Protected by #mutex.
	 */
	IntrusiveList<Downloader> downloaders;

	/**
	 * Protected by #mutex.
	 */
	std::list<ContentDirectoryDescriptor> directories;

	/**
	 * The UPnP device search timeout, which should actually be
	 * called delay because it's the base of a random delay that
	 * the devices apply to avoid responding all at the same time.
	 */
	int search_timeout = 2;

	/**
	 * The time stamp of the last search.
	 */
	std::chrono::steady_clock::time_point last_search = std::chrono::steady_clock::time_point();

public:
	UPnPDeviceDirectory(EventLoop &event_loop, UpnpClient_Handle _handle,
			    UPnPDiscoveryListener *_listener=nullptr);
	~UPnPDeviceDirectory() noexcept;

	UPnPDeviceDirectory(const UPnPDeviceDirectory &) = delete;
	UPnPDeviceDirectory& operator=(const UPnPDeviceDirectory &) = delete;

	EventLoop &GetEventLoop() const noexcept;

	void Start();

	/** Retrieve the directory services currently seen on the network */
	std::vector<ContentDirectoryService> GetDirectories();

	/**
	 * Get server by friendly name.
	 */
	ContentDirectoryService GetServer(std::string_view friendly_name);

private:
	void Search();

	/**
	 * Look at the devices and get rid of those which have not
	 * been seen for too long. We do this when listing the top
	 * directory.
	 *
	 * Caller must lock #mutex.
	 */
	void ExpireDevices();

	void LockAdd(ContentDirectoryDescriptor &&d);
	void LockRemove(const std::string &id);

	int OnAlive(const UpnpDiscovery *disco) noexcept;
	int OnByeBye(const UpnpDiscovery *disco) noexcept;

	/* virtual methods from class UpnpCallback */
	int Invoke(Upnp_EventType et, const void *evp) noexcept override;
};


#endif /* _UPNPPDISC_H_X_INCLUDED_ */
