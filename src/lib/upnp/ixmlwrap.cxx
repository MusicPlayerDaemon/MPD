/* Copyright (C) 2013 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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
