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

#ifndef VOLUME_H
#define VOLUME_H

#include "../config.h"

#include <stdio.h>

#define VOLUME_MIXER_OSS	"oss"
#define VOLUME_MIXER_ALSA	"alsa"
#define VOLUME_MIXER_SOFTWARE	"software"

void initVolume();

void openVolumeDevice();

void finishVolume();

int getVolumeLevel();

int changeVolumeLevel(FILE * fp, int change, int rel);

#endif
