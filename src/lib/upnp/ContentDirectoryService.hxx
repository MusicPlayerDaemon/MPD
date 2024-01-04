// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <upnp.h>

#include <string>
#include <forward_list>

class UPnPDevice;
struct UPnPService;
class UPnPDirContent;

/**
 * Content Directory Service class.
 *
 * This stores identity data from a directory service
 * and the device it belongs to, and has methods to query
 * the directory, using libupnp for handling the UPnP protocols.
 *
 * Note: m_rdreqcnt: number of entries requested per directory read.
 * 0 means all entries. The device can still return less entries than
 * requested, depending on its own limits. In general it's not optimal
 * becauses it triggers issues, and is sometimes actually slower, e.g. on
 * a D-Link NAS 327
 *
 * The value chosen may affect by the UpnpSetMaxContentLength
 * (2000*1024) done during initialization, but this should be ample
 */
class ContentDirectoryService {
	std::string m_actionURL;
	std::string m_serviceType;
	std::string m_deviceId;
	std::string m_friendlyName;

	int m_rdreqcnt; // Slice size to use when reading

public:
	/**
	 * Construct by copying data from device and service objects.
	 *
	 * The discovery service does this: use
	 * UPnPDeviceDirectory::GetDirectories()
	 */
	ContentDirectoryService(const UPnPDevice &device,
				const UPnPService &service) noexcept;

	/** An empty one */
	ContentDirectoryService() = default;

	~ContentDirectoryService() noexcept;

	/** Read a container's children list into dirbuf.
	 *
	 * @param objectId the UPnP object Id for the container. Root has Id "0"
	 */
	UPnPDirContent readDir(UpnpClient_Handle handle,
			       const char *objectId) const;

	void readDirSlice(UpnpClient_Handle handle,
			  const char *objectId, unsigned offset,
			  unsigned count, UPnPDirContent& dirbuf,
			  unsigned &didread, unsigned &total) const;

	/** Search the content directory service.
	 *
	 * @param objectId the UPnP object Id under which the search
	 * should be done. Not all servers actually support this below
	 * root. Root has Id "0"
	 * @param searchstring an UPnP searchcriteria string. Check the
	 * UPnP document: UPnP-av-ContentDirectory-v1-Service-20020625.pdf
	 * section 2.5.5. Maybe we'll provide an easier way some day...
	 */
	UPnPDirContent search(UpnpClient_Handle handle,
			      const char *objectId,
			      const char *searchstring) const;

	/** Read metadata for a given node.
	 *
	 * @param objectId the UPnP object Id. Root has Id "0"
	 */
	UPnPDirContent getMetadata(UpnpClient_Handle handle,
				   const char *objectId) const;

	/** Retrieve search capabilities
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param[out] result an empty vector: no search, or a single '*' element:
	 *     any tag can be used in a search, or a list of usable tag names.
	 */
	std::forward_list<std::string> getSearchCapabilities(UpnpClient_Handle handle) const;

	[[gnu::pure]]
	std::string GetURI() const noexcept {
		return "upnp://" + m_deviceId + "/" + m_serviceType;
	}

	/** Retrieve the "friendly name" for this server, useful for display. */
	const std::string &GetFriendlyName() const noexcept {
		return m_friendlyName;
	}
};
