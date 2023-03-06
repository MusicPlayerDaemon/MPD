// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright J.F.Dockes

#include "config.h"

#ifdef USING_PUPNP
#	include "ixmlwrap.hxx"
#	include "UniqueIxml.hxx"

namespace ixmlwrap {

const char *
getFirstElementValue(IXML_Document *doc, const char *name) noexcept
{
	UniqueIxmlNodeList nodes(ixmlDocument_getElementsByTagName(doc, name));
	if (!nodes)
		return nullptr;

	IXML_Node *first = ixmlNodeList_item(nodes.get(), 0);
	if (!first)
		return nullptr;

	IXML_Node *dnode = ixmlNode_getFirstChild(first);
	if (!dnode)
		return nullptr;

	return ixmlNode_getNodeValue(dnode);
}

} // namespace ixmlwrap
#endif
