/*
* MPD SACD Decoder plugin
* Copyright (c) 2011-2017 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef _SACD_METABASE_H_INCLUDED
#define _SACD_METABASE_H_INCLUDED

#include <string>
#include <upnp/ixml.h>

#include "config.h"
#include "tag/Tag.hxx"
#include "sacd_disc.h"

using namespace std;

#define MB_TAG_ROOT       "root"
#define MB_TAG_STORE      "store"
#define MB_TAG_TRACK      "track"
#define MB_TAG_INFO       "info"
#define MB_TAG_META       "meta"
#define MB_TAG_REPLAYGAIN "replaygain"

#define MB_ATT_ID      "id"
#define MB_ATT_NAME    "name"
#define MB_ATT_TYPE    "type"
#define MB_ATT_VALUE   "value"
#define MB_ATT_VALSEP  ";"
#define MB_ATT_VERSION "version"

#define METABASE_TYPE    "SACD"
#define METABASE_VERSION "1.1"

class sacd_metabase_t {
	string store_id;
	string store_path;
	string store_file;
	string         xml_file;
	IXML_Document* xmldoc;
	bool metabase_loaded;
public:
	sacd_metabase_t(sacd_disc_t* sacd_disc, const char* tags_path = nullptr, const char* tags_file = nullptr);
	~sacd_metabase_t();
    bool get_info(uint32_t track_index, const struct TagHandler& handler, void* handler_ctx);
private:
	bool init_xmldoc();
	IXML_Node* get_track_node(uint32_t track_index);
	void utf2xml(string& src, string& dst);
	void xml2utf(string& src, string& dst);
};

#endif
