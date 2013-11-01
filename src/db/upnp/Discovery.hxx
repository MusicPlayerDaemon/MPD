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

#include "util/Error.hxx"

#include <vector>

#include <time.h>

class ContentDirectoryService;

/**
 * Manage UPnP discovery and maintain a directory of active devices. Singleton.
 *
 * We are only interested in MediaServers with a ContentDirectory service
 * for now, but this could be made more general, by removing the filtering.
 */
class UPnPDeviceDirectory {
	Error error;

	/**
	 * The UPnP device search timeout, which should actually be
	 * called delay because it's the base of a random delay that
	 * the devices apply to avoid responding all at the same time.
	 */
	int m_searchTimeout;

	time_t m_lastSearch;

	UPnPDeviceDirectory();
public:
	UPnPDeviceDirectory(const UPnPDeviceDirectory &) = delete;
	UPnPDeviceDirectory& operator=(const UPnPDeviceDirectory &) = delete;

	/** This class is a singleton. Get the instance here */
	static UPnPDeviceDirectory *getTheDir();

	/** Retrieve the directory services currently seen on the network */
	bool getDirServices(std::vector<ContentDirectoryService> &);

	/**
	 * Get server by friendly name. It's a bit wasteful to copy
	 * all servers for this, we could directly walk the list. Otoh
	 * there isn't going to be millions...
	 */
	bool getServer(const char *friendlyName,
		       ContentDirectoryService &server);

	/** My health */
	bool ok() const {
		return !error.IsDefined();
	}

	/** My diagnostic if health is bad */
	const Error &GetError() const {
		return error;
	}

private:
	bool search();

	/**
	 * Look at the devices and get rid of those which have not
	 * been seen for too long. We do this when listing the top
	 * directory.
	 */
	void expireDevices();
};


#endif /* _UPNPPDISC_H_X_INCLUDED_ */
