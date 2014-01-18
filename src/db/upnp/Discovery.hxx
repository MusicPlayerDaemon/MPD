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

#include "Device.hxx"
#include "WorkQueue.hxx"
#include "thread/Mutex.hxx"
#include "util/Error.hxx"

#include <upnp/upnp.h>

#include <map>
#include <vector>
#include <string>

#include <time.h>

class LibUPnP;
class ContentDirectoryService;

/**
 * Manage UPnP discovery and maintain a directory of active devices. Singleton.
 *
 * We are only interested in MediaServers with a ContentDirectory service
 * for now, but this could be made more general, by removing the filtering.
 */
class UPnPDeviceDirectory {
	/**
	 * Each appropriate discovery event (executing in a libupnp thread
	 * context) queues the following task object for processing by the
	 * discovery thread.
	 */
	struct DiscoveredTask {
		std::string url;
		std::string deviceId;
		int expires; // Seconds valid

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
		UPnPDevice device;
		time_t last_seen;
		int expires; // seconds valid

		ContentDirectoryDescriptor() = default;

		ContentDirectoryDescriptor(time_t last, int exp)
			:last_seen(last), expires(exp+20) {}

		bool Parse(const std::string &url, const char *description,
			   Error &_error) {
			return device.Parse(url, description, _error);
		}
	};

	LibUPnP *const lib;

	Mutex mutex;
	std::map<std::string, ContentDirectoryDescriptor> directories;
	WorkQueue<DiscoveredTask *> discoveredQueue;

	/**
	 * The UPnP device search timeout, which should actually be
	 * called delay because it's the base of a random delay that
	 * the devices apply to avoid responding all at the same time.
	 */
	int m_searchTimeout;

	time_t m_lastSearch;

public:
	UPnPDeviceDirectory(LibUPnP *_lib);

	UPnPDeviceDirectory(const UPnPDeviceDirectory &) = delete;
	UPnPDeviceDirectory& operator=(const UPnPDeviceDirectory &) = delete;

	bool Start(Error &error);

	/** Retrieve the directory services currently seen on the network */
	bool getDirServices(std::vector<ContentDirectoryService> &, Error &);

	/**
	 * Get server by friendly name. It's a bit wasteful to copy
	 * all servers for this, we could directly walk the list. Otoh
	 * there isn't going to be millions...
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
};


#endif /* _UPNPPDISC_H_X_INCLUDED_ */
