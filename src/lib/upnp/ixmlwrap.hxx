// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright J.F.Dockes

#ifndef _IXMLWRAP_H_INCLUDED_
#define _IXMLWRAP_H_INCLUDED_

#ifdef USING_PUPNP
#	include <ixml.h>

namespace ixmlwrap {
	/**
	 * Retrieve the text content for the first element of given
	 * name.  Returns nullptr if the element does not
	 * contain a text node
	 */
	const char *getFirstElementValue(IXML_Document *doc,
					 const char *name) noexcept;

}

#endif /* USING_PUPNP */
#endif /* _IXMLWRAP_H_INCLUDED_ */
