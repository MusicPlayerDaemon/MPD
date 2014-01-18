/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "Domain.hxx"
#include "ContentDirectoryService.hxx"
#include "upnpplib.hxx"

#include <upnp/upnptools.h>

#include <string.h>

// The service type string we are looking for.
static const char *const ContentDirectorySType = "urn:schemas-upnp-org:service:ContentDirectory:1";

// We don't include a version in comparisons, as we are satisfied with
// version 1
gcc_pure
static bool
isCDService(const char *st)
{
	const size_t sz = strlen(ContentDirectorySType) - 2;
	return memcmp(ContentDirectorySType, st, sz) == 0;
}

// The type of device we're asking for in search
static const char *const MediaServerDType = "urn:schemas-upnp-org:device:MediaServer:1";

gcc_pure
static bool
isMSDevice(const char *st)
{
	const size_t sz = strlen(MediaServerDType) - 2;
	return memcmp(MediaServerDType, st, sz) == 0;
}

inline void
UPnPDeviceDirectory::discoExplorer()
{
	for (;;) {
		DiscoveredTask *tsk = 0;
		if (!discoveredQueue.take(tsk)) {
			discoveredQueue.workerExit();
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

		// Update or insert the device
		ContentDirectoryDescriptor d(tsk->url, buf,
					     time(0), tsk->expires);
		free(buf);
		if (!d.device.ok) {
			continue;
		}

		const ScopeLock protect(mutex);
#if defined(__clang__) || GCC_CHECK_VERSION(4,8)
		auto e = directories.emplace(tsk->deviceId, d);
#else
		auto e = directories.insert(std::make_pair(tsk->deviceId, d));
#endif
		if (!e.second)
			e.first->second = d;

		delete tsk;
	}
}

void *
UPnPDeviceDirectory::discoExplorer(void *ctx)
{
	UPnPDeviceDirectory &directory = *(UPnPDeviceDirectory *)ctx;
	directory.discoExplorer();
	return (void*)1;
}

inline int
UPnPDeviceDirectory::OnAlive(Upnp_Discovery *disco)
{
	if (isMSDevice(disco->DeviceType) ||
	    isCDService(disco->ServiceType)) {
		DiscoveredTask *tp = new DiscoveredTask(disco);
		if (discoveredQueue.put(tp))
			return UPNP_E_FINISH;
	}

	return UPNP_E_SUCCESS;
}

inline int
UPnPDeviceDirectory::OnByeBye(Upnp_Discovery *disco)
{

	if (isMSDevice(disco->DeviceType) ||
	    isCDService(disco->ServiceType)) {
		// Device signals it is going off.
		const ScopeLock protect(mutex);
		auto it = directories.find(disco->DeviceId);
		if (it != directories.end())
			directories.erase(it);
	}

	return UPNP_E_SUCCESS;
}

// This gets called for all libupnp asynchronous events, in a libupnp
// thread context.
// Example: ContentDirectories appearing and disappearing from the network
// We queue a task for our worker thread(s)
inline int
UPnPDeviceDirectory::cluCallBack(Upnp_EventType et, void *evp)
{
	switch (et) {
	case UPNP_DISCOVERY_SEARCH_RESULT:
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		{
			Upnp_Discovery *disco = (Upnp_Discovery *)evp;
			return OnAlive(disco);
		}

	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
		{
			Upnp_Discovery *disco = (Upnp_Discovery *)evp;
			return OnByeBye(disco);
		}

	default:
		// Ignore other events for now
		break;
	}

	return UPNP_E_SUCCESS;
}

void
UPnPDeviceDirectory::expireDevices()
{
	const ScopeLock protect(mutex);
	time_t now = time(0);
	bool didsomething = false;

	for (auto it = directories.begin();
	     it != directories.end();) {
		if (now - it->second.last_seen > it->second.expires) {
			it = directories.erase(it);
			didsomething = true;
		} else {
			it++;
		}
	}

	if (didsomething)
		search();
}

UPnPDeviceDirectory::UPnPDeviceDirectory(LibUPnP *_lib)
	:lib(_lib),
	 discoveredQueue("DiscoveredQueue"),
	 m_searchTimeout(2), m_lastSearch(0)
{
	if (!discoveredQueue.start(1, discoExplorer, this)) {
		error.Set(upnp_domain, "Discover work queue start failed");
		return;
	}

	lib->SetHandler([this](Upnp_EventType type, void *event){
			cluCallBack(type, event);
		});

	search();
}

bool
UPnPDeviceDirectory::search()
{
	time_t now = time(0);
	if (now - m_lastSearch < 10)
		return true;
	m_lastSearch = now;

	// We search both for device and service just in case.
	int code = UpnpSearchAsync(lib->getclh(), m_searchTimeout,
				   ContentDirectorySType, lib);
	if (code != UPNP_E_SUCCESS) {
		error.Format(upnp_domain, code,
			     "UpnpSearchAsync() failed: %s",
			     UpnpGetErrorMessage(code));
		return false;
	}

	code = UpnpSearchAsync(lib->getclh(), m_searchTimeout,
			       MediaServerDType, lib);
	if (code != UPNP_E_SUCCESS) {
		error.Format(upnp_domain, code,
			     "UpnpSearchAsync() failed: %s",
			     UpnpGetErrorMessage(code));
		return false;
	}

	return true;
}

bool
UPnPDeviceDirectory::getDirServices(std::vector<ContentDirectoryService> &out)
{
	if (!ok())
		return false;

	// Has locking, do it before our own lock
	expireDevices();

	const ScopeLock protect(mutex);

	for (auto dit = directories.begin();
	     dit != directories.end(); dit++) {
		for (const auto &service : dit->second.device.services) {
			if (isCDService(service.serviceType.c_str())) {
				out.emplace_back(dit->second.device, service);
			}
		}
	}

	return true;
}

bool
UPnPDeviceDirectory::getServer(const char *friendlyName,
			       ContentDirectoryService &server)
{
	std::vector<ContentDirectoryService> ds;
	if (!getDirServices(ds)) {
		return false;
	}

	for (const auto &i : ds) {
		if (strcmp(friendlyName, i.getFriendlyName()) == 0) {
			server = i;
			return true;
		}
	}

	return false;
}
