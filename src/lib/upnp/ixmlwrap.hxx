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
