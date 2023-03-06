// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_UPNP_UNIQUE_XML_HXX
#define MPD_UPNP_UNIQUE_XML_HXX

#ifdef USING_PUPNP
#	include <ixml.h>

#	include <memory>

struct UpnpIxmlDeleter {
	void operator()(IXML_Document *doc) noexcept {
		ixmlDocument_free(doc);
	}

	void operator()(IXML_NodeList *nl) noexcept {
		ixmlNodeList_free(nl);
	}
};

typedef std::unique_ptr<IXML_Document, UpnpIxmlDeleter> UniqueIxmlDocument;
typedef std::unique_ptr<IXML_NodeList, UpnpIxmlDeleter> UniqueIxmlNodeList;

#endif /* USING_PUPNP */
#endif
