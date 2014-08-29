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

#ifndef _UPNPPDISC_H_X_INCLUDED_
#define _UPNPPDISC_H_X_INCLUDED_

#include "Callback.hxx"
#include "Device.hxx"
#include "WorkQueue.hxx"
#include "thread/Mutex.hxx"
#include "util/Error.hxx"
#include "Compiler.h"

#include <upnp/upnp.h>

#include <list>
#include <vector>
#include <string>

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
	 * Each appropriate discovery event (executing in a libupnp thread
	 * context) queues the following task object for processing by the
	 * discovery thread.
	 */
	struct DiscoveredTask {
		std::string url;
		std::string deviceId;
		unsigned expires; // Seconds valid

		DiscoveredTask(const Upnp_Discovery *disco)
			:url(disco->Location),
			  deviceId(disco->DeviceId),
			  expires(disco->Expires) {}
	};

	/**
	 * Descriptor for one device having a Content Directory
	 * service found on the network.
	 */
	class ContentDirectoryDescriptor {
	public:
		std::string id;

		UPnPDevice device;

		/**
		 * The MonotonicClockS() time stamp when this device
		 * expires.
		 */
		unsigned expires;

		ContentDirectoryDescriptor() = default;

		ContentDirectoryDescriptor(std::string &&_id,
					   unsigned last, int exp)
			:id(std::move(_id)), expires(last + exp + 20) {}

		bool Parse(const std::string &url, const char *description,
			   Error &_error) {
			return device.Parse(url, description, _error);
		}
	};

	const UpnpClient_Handle handle;
	UPnPDiscoveryListener *const listener;

	Mutex mutex;
	std::list<ContentDirectoryDescriptor> directories;
	WorkQueue<DiscoveredTask *> discoveredQueue;

	/**
	 * The UPnP device search timeout, which should actually be
	 * called delay because it's the base of a random delay that
	 * the devices apply to avoid responding all at the same time.
	 */
	int m_searchTimeout;

	/**
	 * The MonotonicClockS() time stamp of the last search.
	 */
	unsigned m_lastSearch;

public:
	UPnPDeviceDirectory(UpnpClient_Handle _handle,
			    UPnPDiscoveryListener *_listener=nullptr);
	~UPnPDeviceDirectory();

	UPnPDeviceDirectory(const UPnPDeviceDirectory &) = delete;
	UPnPDeviceDirectory& operator=(const UPnPDeviceDirectory &) = delete;

	bool Start(Error &error);

	/** Retrieve the directory services currently seen on the network */
	bool getDirServices(std::vector<ContentDirectoryService> &, Error &);

	/**
	 * Get server by friendly name.
	 */
	bool getServer(const char *friendlyName,
		       ContentDirectoryService &server,
		       Error &error);

private:
	bool search(Error &error);

	/**
	 * Look at the devices and get rid of those which have not
	 * been seen for too long. We do this when listing the top
	 * directory.
	 */
	bool expireDevices(Error &error);

	void LockAdd(ContentDirectoryDescriptor &&d);
	void LockRemove(const std::string &id);

	/**
	 * Worker routine for the discovery queue. Get messages about
	 * devices appearing and disappearing, and update the
	 * directory pool accordingly.
	 */
	static void *discoExplorer(void *);
	void discoExplorer();

	int OnAlive(Upnp_Discovery *disco);
	int OnByeBye(Upnp_Discovery *disco);
	int cluCallBack(Upnp_EventType et, void *evp);

	/* virtual methods from class UpnpCallback */
	virtual int Invoke(Upnp_EventType et, void *evp) override;
};


#endif /* _UPNPPDISC_H_X_INCLUDED_ */
