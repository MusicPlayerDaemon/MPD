// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "lib/upnp/ContentDirectoryService.hxx"
#include "lib/upnp/Action.hxx"
#include "Directory.hxx"
#include "util/CNumberParser.hxx"

#include <fmt/format.h>

static void
ReadResultTag(UPnPDirContent &dirbuf, const UpnpActionResponse &response)
{
	const char *p = response.GetValue("Result");
	if (p == nullptr)
		p = "";

	dirbuf.Parse(p);
}

inline void
ContentDirectoryService::readDirSlice(UpnpClient_Handle hdl,
				      const char *objectId, unsigned offset,
				      unsigned count, UPnPDirContent &dirbuf,
				      unsigned &didreadp,
				      unsigned &totalp) const
{
	// Some devices require an empty SortCriteria, else bad params
	const auto response = UpnpSendAction(hdl, m_actionURL.c_str(),
					     "Browse", m_serviceType.c_str(),
					     {
						     {"ObjectID", objectId},
						     {"BrowseFlag", "BrowseDirectChildren"},
						     {"Filter", "*"},
						     {"SortCriteria", ""},
						     {"StartingIndex", fmt::format_int{offset}.c_str()},
						     {"RequestedCount", fmt::format_int{count}.c_str()},
					     });

	const char *value = response.GetValue("NumberReturned");
	didreadp = value != nullptr
		? ParseUnsigned(value)
		: 0;

	value = response.GetValue("TotalMatches");
	if (value != nullptr)
		totalp = ParseUnsigned(value);

	ReadResultTag(dirbuf, response);
}

/**
 * The maximum number of objects in one container.  The loop
 * termination depends on numbers reported by the (untrusted)
 * server, and this limit protects against servers which never stop
 * sending objects.
 */
static constexpr std::size_t MAX_OBJECTS = 256 * 1024;

UPnPDirContent
ContentDirectoryService::readDir(UpnpClient_Handle handle,
				 const char *objectId) const
{
	UPnPDirContent dirbuf;
	unsigned offset = 0, total = -1, count;
	std::size_t old_size;

	do {
		old_size = dirbuf.objects.size();
		readDirSlice(handle, objectId, offset, m_rdreqcnt, dirbuf,
			     count, total);

		offset += count;
	} while (count > 0 && offset < total &&
		 /* stop if the server sends no objects */
		 dirbuf.objects.size() > old_size &&
		 dirbuf.objects.size() < MAX_OBJECTS);

	return dirbuf;
}

UPnPDirContent
ContentDirectoryService::search(UpnpClient_Handle hdl,
				const char *objectId,
				const char *ss) const
{
	UPnPDirContent dirbuf;
	unsigned offset = 0, total = -1, count;
	std::size_t old_size;

	do {
		old_size = dirbuf.objects.size();

		const auto response = UpnpSendAction(hdl, m_actionURL.c_str(),
						     "Search", m_serviceType.c_str(),
						     {
							     {"ContainerID", objectId},
							     {"SearchCriteria", ss},
							     {"Filter", "*"},
							     {"SortCriteria", ""},
							     {"StartingIndex", fmt::format_int{offset}.c_str()},
							     {"RequestedCount", "0"}, // Setting a value here gets twonky into fits
						     });

		const char *value = response.GetValue("NumberReturned");
		count = value != nullptr
			? ParseUnsigned(value)
			: 0;

		offset += count;

		value = response.GetValue("TotalMatches");
		if (value != nullptr)
			total = ParseUnsigned(value);

		ReadResultTag(dirbuf, response);
	} while (count > 0 && offset < total &&
		 /* stop if the server sends no objects */
		 dirbuf.objects.size() > old_size &&
		 dirbuf.objects.size() < MAX_OBJECTS);

	return dirbuf;
}

UPnPDirContent
ContentDirectoryService::getMetadata(UpnpClient_Handle hdl,
				     const char *objectId) const
{
	const auto response = UpnpSendAction(hdl, m_actionURL.c_str(),
					     "Browse", m_serviceType.c_str(),
					     {
						     {"ObjectID", objectId},
						     {"BrowseFlag", "BrowseMetadata"},
						     {"Filter", "*"},
						     {"SortCriteria", ""},
						     {"StartingIndex", "0"},
						     {"RequestedCount", "1"},
					     });

	UPnPDirContent dirbuf;
	ReadResultTag(dirbuf, response);
	return dirbuf;
}
