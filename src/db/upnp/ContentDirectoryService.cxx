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
#include "ContentDirectoryService.hxx"
#include "Domain.hxx"
#include "Device.hxx"
#include "ixmlwrap.hxx"
#include "Directory.hxx"
#include "Util.hxx"
#include "Action.hxx"
#include "util/Error.hxx"

#include <string.h>
#include <stdio.h>

ContentDirectoryService::ContentDirectoryService(const UPnPDevice &device,
						 const UPnPService &service)
	:m_actionURL(caturl(device.URLBase, service.controlURL)),
	 m_serviceType(service.serviceType),
	 m_deviceId(device.UDN),
	 m_friendlyName(device.friendlyName),
	 m_manufacturer(device.manufacturer),
	 m_modelName(device.modelName),
	 m_rdreqcnt(200)
{
	if (!m_modelName.compare("MediaTomb")) {
		// Readdir by 200 entries is good for most, but MediaTomb likes
		// them really big. Actually 1000 is better but I don't dare
		m_rdreqcnt = 500;
	}
}

class DirBResFree {
public:
	IXML_Document **rspp;
	DirBResFree(IXML_Document **_rspp)
		:rspp(_rspp) {}
	~DirBResFree()
	{
		if (*rspp)
			ixmlDocument_free(*rspp);
	}
};

static bool
ReadResultTag(UPnPDirContent &dirbuf, IXML_Document *response, Error &error)
{
	const char *p = ixmlwrap::getFirstElementValue(response, "Result");
	if (p == nullptr)
		p = "";

	return dirbuf.parse(p, error);
}

bool
ContentDirectoryService::readDirSlice(UpnpClient_Handle hdl,
				      const char *objectId, int offset,
				      int count, UPnPDirContent &dirbuf,
				      int *didreadp, int *totalp,
				      Error &error)
{
	// Create request
	char ofbuf[100], cntbuf[100];
	sprintf(ofbuf, "%d", offset);
	sprintf(cntbuf, "%d", count);
	// Some devices require an empty SortCriteria, else bad params
	IXML_Document *request =
		MakeActionHelper("Browse", m_serviceType.c_str(),
				 "ObjectID", objectId,
				 "BrowseFlag", "BrowseDirectChildren",
				 "Filter", "*",
				 "SortCriteria", "",
				 "StartingIndex", ofbuf,
				 "RequestedCount", cntbuf);
	if (request == nullptr) {
		error.Set(upnp_domain, "UpnpMakeAction() failed");
		return false;
	}

	IXML_Document *response;
	int code = UpnpSendAction(hdl, m_actionURL.c_str(), m_serviceType.c_str(),
				  0 /*devUDN*/, request, &response);
	ixmlDocument_free(request);
	if (code != UPNP_E_SUCCESS) {
		error.Format(upnp_domain, code,
			     "UpnpSendAction() failed: %s",
			     UpnpGetErrorMessage(code));
		return false;
	}

	DirBResFree cleaner(&response);

	int didread = -1;
	const char *value = ixmlwrap::getFirstElementValue(response, "NumberReturned");
	if (value != nullptr)
		didread = atoi(value);

	if (count == -1 || count == 0) {
		// TODO: what's this?
		error.Set(upnp_domain, "got -1 or 0 entries");
		return false;
	}

	value = ixmlwrap::getFirstElementValue(response, "TotalMatches");
	if (value != nullptr)
		*totalp = atoi(value);

	if (!ReadResultTag(dirbuf, response, error))
		return false;

	*didreadp = didread;
	return true;
}

bool
ContentDirectoryService::readDir(UpnpClient_Handle handle,
				 const char *objectId,
				 UPnPDirContent &dirbuf,
				 Error &error)
{
	int offset = 0;
	int total = 1000;// Updated on first read.

	while (offset < total) {
		int count;
		if (!readDirSlice(handle, objectId, offset, m_rdreqcnt, dirbuf,
				  &count, &total, error))
			return false;

		offset += count;
	}

	return true;
}

bool
ContentDirectoryService::search(UpnpClient_Handle hdl,
				const char *objectId,
				const char *ss,
				UPnPDirContent &dirbuf,
				Error &error)
{
	int offset = 0;
	int total = 1000;// Updated on first read.

	while (offset < total) {
		char ofbuf[100];
		sprintf(ofbuf, "%d", offset);

		IXML_Document *request =
			MakeActionHelper("Search", m_serviceType.c_str(),
					 "ContainerID", objectId,
					 "SearchCriteria", ss,
					 "Filter", "*",
					 "SortCriteria", "",
					 "StartingIndex", ofbuf,
					 "RequestedCount", "0"); // Setting a value here gets twonky into fits
		if (request == 0) {
			error.Set(upnp_domain, "UpnpMakeAction() failed");
			return false;
		}

		IXML_Document *response;
		auto code = UpnpSendAction(hdl, m_actionURL.c_str(),
					   m_serviceType.c_str(),
					   0 /*devUDN*/, request, &response);
		ixmlDocument_free(request);
		if (code != UPNP_E_SUCCESS) {
			error.Format(upnp_domain, code,
				     "UpnpSendAction() failed: %s",
				     UpnpGetErrorMessage(code));
			return false;
		}

		DirBResFree cleaner(&response);

		int count = -1;
		const char *value =
			ixmlwrap::getFirstElementValue(response, "NumberReturned");
		if (value != nullptr)
			count = atoi(value);

		if (count == -1 || count == 0) {
			// TODO: what's this?
			error.Set(upnp_domain, "got -1 or 0 entries");
			return false;
		}

		offset += count;

		value = ixmlwrap::getFirstElementValue(response, "TotalMatches");
		if (value != nullptr)
			total = atoi(value);

		if (!ReadResultTag(dirbuf, response, error))
			return false;
	}

	return true;
}

bool
ContentDirectoryService::getSearchCapabilities(UpnpClient_Handle hdl,
					       std::set<std::string> &result,
					       Error &error)
{
	assert(result.empty());

	IXML_Document *request =
		UpnpMakeAction("GetSearchCapabilities", m_serviceType.c_str(),
			       0,
			       nullptr, nullptr);
	if (request == 0) {
		error.Set(upnp_domain, "UpnpMakeAction() failed");
		return false;
	}

	IXML_Document *response;
	auto code = UpnpSendAction(hdl, m_actionURL.c_str(),
				   m_serviceType.c_str(),
				   0 /*devUDN*/, request, &response);
	ixmlDocument_free(request);
	if (code != UPNP_E_SUCCESS) {
		error.Format(upnp_domain, code,
			     "UpnpSendAction() failed: %s",
			     UpnpGetErrorMessage(code));
		return false;
	}

	const char *s = ixmlwrap::getFirstElementValue(response, "SearchCaps");
	if (s == nullptr || *s == 0) {
		ixmlDocument_free(response);
		return true;
	}

	bool success = true;
	if (strcmp(s, "*") == 0) {
		result.insert(result.end(), "*");
	} else if (!csvToStrings(s, result)) {
		error.Set(upnp_domain, "Bad response");
		success = false;
	}

	ixmlDocument_free(response);
	return success;
}

bool
ContentDirectoryService::getMetadata(UpnpClient_Handle hdl,
				     const char *objectId,
				     UPnPDirContent &dirbuf,
				     Error &error)
{
	// Create request
	IXML_Document *request =
		MakeActionHelper("Browse", m_serviceType.c_str(),
				 "ObjectID", objectId,
				 "BrowseFlag", "BrowseMetadata",
				 "Filter", "*",
				 "SortCriteria", "",
				 "StartingIndex", "0",
				 "RequestedCount", "1");
	if (request == nullptr) {
		error.Set(upnp_domain, "UpnpMakeAction() failed");
		return false;
	}

	IXML_Document *response;
	auto code = UpnpSendAction(hdl, m_actionURL.c_str(),
				   m_serviceType.c_str(),
				   0 /*devUDN*/, request, &response);
	ixmlDocument_free(request);
	if (code != UPNP_E_SUCCESS) {
		error.Format(upnp_domain, code,
			     "UpnpSendAction() failed: %s",
			     UpnpGetErrorMessage(code));
		return false;
	}

	bool success = ReadResultTag(dirbuf, response, error);
	ixmlDocument_free(response);
	return success;
}
