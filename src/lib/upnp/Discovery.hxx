// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Callback.hxx"
#include "lib/curl/Init.hxx"
#include "thread/Mutex.hxx"
#include "util/IntrusiveList.hxx"

#include <map>
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
	class ContentDirectoryDescriptor;

	class Downloader;

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
	std::map<std::string, ContentDirectoryDescriptor, std::less<>> directories;

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

	[[gnu::const]]
	EventLoop &GetEventLoop() const noexcept;

	void Start();

	/** Retrieve the directory services currently seen on the network */
	[[gnu::pure]]
	std::vector<ContentDirectoryService> GetDirectories() noexcept;

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
	void ExpireDevices() noexcept;

	void LockAdd(std::string &&id, ContentDirectoryDescriptor &&d) noexcept;
	void LockRemove(std::string_view id) noexcept;

	int OnAlive(const UpnpDiscovery *disco) noexcept;
	int OnByeBye(const UpnpDiscovery *disco) noexcept;

	/* virtual methods from class UpnpCallback */
	int Invoke(Upnp_EventType et, const void *evp) noexcept override;
};

