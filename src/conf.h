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

#ifndef CONF_H
#define CONF_H

#include "../config.h"

#define CONF_PORT				0
#define CONF_MUSIC_DIRECTORY			1
#define CONF_PLAYLIST_DIRECTORY			2
#define CONF_LOG_FILE				3
#define CONF_ERROR_FILE				4
#define CONF_CONNECTION_TIMEOUT			5
#define CONF_MIXER_DEVICE			6
#define CONF_MAX_CONNECTIONS			7
#define CONF_MAX_PLAYLIST_LENGTH		8
#define CONF_BUFFER_BEFORE_PLAY			9
#define CONF_MAX_COMMAND_LIST_SIZE		10
#define CONF_MAX_OUTPUT_BUFFER_SIZE		11
#define CONF_AO_DRIVER				12
#define CONF_AO_DRIVER_OPTIONS			13
#define CONF_SAVE_ABSOLUTE_PATHS_IN_PLAYLISTS	14
#define CONF_BIND_TO_ADDRESS			15
#define CONF_MIXER_TYPE				16
#define CONF_STATE_FILE				17
#define CONF_USER				18
#define CONF_DB_FILE				19
#define CONF_LOG_LEVEL				20
#define CONF_MIXER_CONTROL			21
#define CONF_AUDIO_WRITE_SIZE			22
#define CONF_FS_CHARSET				23
#define CONF_PASSWORD				24
#define CONF_DEFAULT_PERMISSIONS		25
#define CONF_BUFFER_SIZE			26
#define CONF_REPLAYGAIN                         27
#define CONF_AUDIO_OUTPUT_FORMAT                28
#define CONF_HTTP_PROXY_HOST                    29
#define CONF_HTTP_PROXY_PORT                    30
#define CONF_HTTP_PROXY_USER			31
#define CONF_HTTP_PROXY_PASSWORD		32
#define CONF_REPLAYGAIN_PREAMP			33
#define CONF_ID3V1_ENCODING			34

#define CONF_CAT_CHAR				"\n"

/* do not free the return value, it is a static variable */
char ** readConf(char * file);

char ** getConf();

void initConf();

void writeConf(char * file);

#endif
