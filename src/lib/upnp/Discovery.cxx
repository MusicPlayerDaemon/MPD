/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "config.h"
#include "Discovery.hxx"
#include "ContentDirectoryService.hxx"
#include "Log.hxx"
#include "util/ScopeExit.hxx"
#include "util/RuntimeError.hxx"

#include <upnp/upnptools.h>

#include <stdlib.h>
#include <string.h>

// The service type string we are looking for.
static constexpr char ContentDirectorySType[] = "urn:schemas-upnp-org:service:ContentDirectory:1";

// We don't include a version in comparisons, as we are satisfied with
// version 1
gcc_pure
static bool
isCDService(const char *st) noexcept
{
	constexpr size_t sz = sizeof(ContentDirectorySType) - 3;
	return memcmp(ContentDirectorySType, st, sz) == 0;
}

// The type of device we're asking for in search
static constexpr char MediaServerDType[] = "urn:schemas-upnp-org:device:MediaServer:1";

gcc_pure
static bool
isMSDevice(const char *st) noexcept
{
	constexpr size_t sz = sizeof(MediaServerDType) - 3;
	return memcmp(MediaServerDType, st, sz) == 0;
}

static void
AnnounceFoundUPnP(UPnPDiscoveryListener &listener, const UPnPDevice &device)
{
	for (const auto &service : device.services)
		if (isCDService(service.serviceType.c_str()))
			listener.FoundUPnP(ContentDirectoryService(device,
								   service));
}

static void
AnnounceLostUPnP(UPnPDiscoveryListener &listener, const UPnPDevice &device)
{
	for (const auto &service : device.services)
		if (isCDService(service.serviceType.c_str()))
			listener.LostUPnP(ContentDirectoryService(device,
								  service));
}

inline void
UPnPDeviceDirectory::LockAdd(ContentDirectoryDescriptor &&d)
{
	const std::lock_guard<Mutex> protect(mutex);

	for (auto &i : directories) {
		if (i.id == d.id) {
			i = std::move(d);
			return;
		}
	}

	directories.emplace_back(std::move(d));

	if (listener != nullptr)
		AnnounceFoundUPnP(*listener, directories.back().device);
}

inline void
UPnPDeviceDirectory::LockRemove(const std::string &id)
{
	const std::lock_guard<Mutex> protect(mutex);

	for (auto i = directories.begin(), end = directories.end();
	     i != end; ++i) {
		if (i->id == id) {
			if (listener != nullptr)
				AnnounceLostUPnP(*listener, i->device);

			directories.erase(i);
			break;
		}
	}
}

inline void
UPnPDeviceDirectory::Explore()
{
	for (;;) {
		std::unique_ptr<DiscoveredTask> tsk;
		if (!queue.take(tsk)) {
			queue.workerExit();
			return;
		}

		// Device signals its existence and well-being. Perform the
		// UPnP "description" phase by downloading and decoding the
		// description document.
		char *buf;
		// LINE_SIZE is defined by libupnp's upnp.h...
		char contentType[LINE_SIZE];
		int code = UpnpDownloadUrlItem(tsk->url.c_str(), &buf, contentType);
		if (code != UPNP_E_SUCCESS) {
			continue;
		}

		AtScopeExit(buf){ free(buf); };

		// Update or insert the device
		ContentDirectoryDescriptor d(std::move(tsk->device_id),
					     std::chrono::steady_clock::now(),
					     tsk->expires);

		try {
			d.Parse(tsk->url, buf);
		} catch (const std::exception &e) {
			LogError(e);
		}

		LockAdd(std::move(d));
	}
}

void *
UPnPDeviceDirectory::Explore(void *ctx)
{
	UPnPDeviceDirectory &directory = *(UPnPDeviceDirectory *)ctx;
	directory.Explore();
	return (void*)1;
}

inline int
UPnPDeviceDirectory::OnAlive(const Upnp_Discovery *disco)
{
	if (isMSDevice(disco->DeviceType) ||
	    isCDService(disco->ServiceType)) {
		DiscoveredTask *tp = new DiscoveredTask(disco);
		if (queue.put(tp))
			return UPNP_E_FINISH;
	}

	return UPNP_E_SUCCESS;
}

inline int
UPnPDeviceDirectory::OnByeBye(const Upnp_Discovery *disco)
{
	if (isMSDevice(disco->DeviceType) ||
	    isCDService(disco->ServiceType)) {
		// Device signals it is going off.
		LockRemove(disco->DeviceId);
	}

	return UPNP_E_SUCCESS;
}

// This gets called for all libupnp asynchronous events, in a libupnp
// thread context.
// Example: ContentDirectories appearing and disappearing from the network
// We queue a task for our worker thread(s)
int
UPnPDeviceDirectory::Invoke(Upnp_EventType et, const void *evp)
{
	switch (et) {
	case UPNP_DISCOVERY_SEARCH_RESULT:
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		{
			auto *disco = (const Upnp_Discovery *)evp;
			return OnAlive(disco);
		}

	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
		{
			auto *disco = (const Upnp_Discovery *)evp;
			return OnByeBye(disco);
		}

	default:
		// Ignore other events for now
		break;
	}

	return UPNP_E_SUCCESS;
}

void
UPnPDeviceDirectory::ExpireDevices()
{
	const auto now = std::chrono::steady_clock::now();
	bool didsomething = false;

	for (auto it = directories.begin();
	     it != directories.end();) {
		if (now > it->expires) {
			it = directories.erase(it);
			didsomething = true;
		} else {
			it++;
		}
	}

	if (didsomething)
		Search();
}

UPnPDeviceDirectory::UPnPDeviceDirectory(UpnpClient_Handle _handle,
					 UPnPDiscoveryListener *_listener)
	:handle(_handle),
	 listener(_listener),
	 queue("DiscoveredQueue")
{
}

UPnPDeviceDirectory::~UPnPDeviceDirectory()
{
	/* this destructor exists here just so it won't get inlined */
}

void
UPnPDeviceDirectory::Start()
{
	if (!queue.start(1, Explore, this))
		throw std::runtime_error("Discover work queue start failed");

	Search();
}

void
UPnPDeviceDirectory::Search()
{
	const auto now = std::chrono::steady_clock::now();
	if (now - last_search < std::chrono::seconds(10))
		return;
	last_search = now;

	// We search both for device and service just in case.
	int code = UpnpSearchAsync(handle, search_timeout,
				   ContentDirectorySType, GetUpnpCookie());
	if (code != UPNP_E_SUCCESS)
		throw FormatRuntimeError("UpnpSearchAsync() failed: %s",
					 UpnpGetErrorMessage(code));

	code = UpnpSearchAsync(handle, search_timeout,
			       MediaServerDType, GetUpnpCookie());
	if (code != UPNP_E_SUCCESS)
		throw FormatRuntimeError("UpnpSearchAsync() failed: %s",
					 UpnpGetErrorMessage(code));
}

std::vector<ContentDirectoryService>
UPnPDeviceDirectory::GetDirectories()
{
	const std::lock_guard<Mutex> protect(mutex);

	ExpireDevices();

	std::vector<ContentDirectoryService> out;
	for (auto dit = directories.begin();
	     dit != directories.end(); dit++) {
		for (const auto &service : dit->device.services) {
			if (isCDService(service.serviceType.c_str())) {
				out.emplace_back(dit->device, service);
			}
		}
	}

	return out;
}

ContentDirectoryService
UPnPDeviceDirectory::GetServer(const char *friendly_name)
{
	const std::lock_guard<Mutex> protect(mutex);

	ExpireDevices();

	for (const auto &i : directories) {
		const auto &device = i.device;

		if (device.friendlyName != friendly_name)
			continue;

		for (const auto &service : device.services)
			if (isCDService(service.serviceType.c_str()))
				return ContentDirectoryService(device,
							       service);
	}

	throw std::runtime_error("Server not found");
}
