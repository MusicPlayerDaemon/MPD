/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef OGG_DECODE_H
#define OGG_DECODE_H

#include "../config.h"

#include "playerData.h"
#include "inputStream.h"

#include <stdio.h>

int ogg_decode(OutputBuffer * cb, DecoderControl * dc, InputStream * inStream);

int getOggTotalTime(char * file);

#endif
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
