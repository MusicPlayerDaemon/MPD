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
#include "Device.hxx"
#include "Domain.hxx"
#include "ContentDirectoryService.hxx"
#include "WorkQueue.hxx"
#include "upnpplib.hxx"
#include "thread/Mutex.hxx"

#include <upnp/upnp.h>
#include <upnp/upnptools.h>

#include <string.h>

#include <map>

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

/**
 * Each appropriate discovery event (executing in a libupnp thread
 * context) queues the following task object for processing by the
 * discovery thread.
 */
struct DiscoveredTask {
	bool alive;
	std::string url;
	std::string deviceId;
	int expires; // Seconds valid

	DiscoveredTask(bool _alive, const Upnp_Discovery *disco)
		: alive(_alive), url(disco->Location),
		  deviceId(disco->DeviceId),
		  expires(disco->Expires) {}

};
static WorkQueue<DiscoveredTask *> discoveredQueue("DiscoveredQueue");

// Descriptor for one device having a Content Directory service found
// on the network.
class ContentDirectoryDescriptor {
public:
	ContentDirectoryDescriptor(const std::string &url,
				   const std::string &description,
				   time_t last, int exp)
		:device(url, description), last_seen(last), expires(exp+20) {}
	UPnPDevice device;
	time_t last_seen;
	int expires; // seconds valid
};

// A ContentDirectoryPool holds the characteristics of the servers
// currently on the network.
// The map is referenced by deviceId (==UDN)
// The class is instanciated as a static (unenforced) singleton.
class ContentDirectoryPool {
public:
	Mutex m_mutex;
	std::map<std::string, ContentDirectoryDescriptor> m_directories;
};

static ContentDirectoryPool contentDirectories;

// Worker routine for the discovery queue. Get messages about devices
// appearing and disappearing, and update the directory pool
// accordingly.
static void *
discoExplorer(void *)
{
	for (;;) {
		DiscoveredTask *tsk = 0;
		if (!discoveredQueue.take(tsk)) {
			discoveredQueue.workerExit();
			return (void*)1;
		}

		const ScopeLock protect(contentDirectories.m_mutex);
		if (!tsk->alive) {
			// Device signals it is going off.
			auto it = contentDirectories.m_directories.find(tsk->deviceId);
			if (it != contentDirectories.m_directories.end()) {
				contentDirectories.m_directories.erase(it);
			}
		} else {
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
			std::string sdesc(buf);

			// Update or insert the device
			ContentDirectoryDescriptor d(tsk->url, sdesc,
						     time(0), tsk->expires);
			if (!d.device.ok) {
				continue;
			}

#if defined(__clang__) || GCC_CHECK_VERSION(4,8)
			auto e = contentDirectories.m_directories.emplace(tsk->deviceId, d);
#else
			auto e = contentDirectories.m_directories.insert(std::make_pair(tsk->deviceId, d));
#endif
			if (!e.second)
				e.first->second = d;
		}
		delete tsk;
	}
}

// This gets called for all libupnp asynchronous events, in a libupnp
// thread context.
// Example: ContentDirectories appearing and disappearing from the network
// We queue a task for our worker thread(s)
static int
cluCallBack(Upnp_EventType et, void *evp)
{
	switch (et) {
	case UPNP_DISCOVERY_SEARCH_RESULT:
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		{
			Upnp_Discovery *disco = (Upnp_Discovery *)evp;
			if (isMSDevice(disco->DeviceType) ||
			    isCDService(disco->ServiceType)) {
				DiscoveredTask *tp = new DiscoveredTask(1, disco);
				if (discoveredQueue.put(tp))
					return UPNP_E_FINISH;
			}
			break;
		}

	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
		{
			Upnp_Discovery *disco = (Upnp_Discovery *)evp;

			if (isMSDevice(disco->DeviceType) ||
			    isCDService(disco->ServiceType)) {
				DiscoveredTask *tp = new DiscoveredTask(0, disco);
				if (discoveredQueue.put(tp))
					return UPNP_E_FINISH;
			}
			break;
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
	const ScopeLock protect(contentDirectories.m_mutex);
	time_t now = time(0);
	bool didsomething = false;

	for (auto it = contentDirectories.m_directories.begin();
	     it != contentDirectories.m_directories.end();) {
		if (now - it->second.last_seen > it->second.expires) {
			it = contentDirectories.m_directories.erase(it);
			didsomething = true;
		} else {
			it++;
		}
	}

	if (didsomething)
		search();
}

UPnPDeviceDirectory::UPnPDeviceDirectory(LibUPnP *_lib)
	:lib(_lib), m_searchTimeout(2), m_lastSearch(0)
{
	if (!discoveredQueue.start(1, discoExplorer, 0)) {
		error.Set(upnp_domain, "Discover work queue start failed");
		return;
	}

	lib->SetHandler([](Upnp_EventType type, void *event){
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

	const ScopeLock protect(contentDirectories.m_mutex);

	for (auto dit = contentDirectories.m_directories.begin();
	     dit != contentDirectories.m_directories.end(); dit++) {
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
