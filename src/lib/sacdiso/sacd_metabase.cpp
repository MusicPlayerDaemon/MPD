/*
* MPD SACD Decoder plugin
* Copyright (c) 2011-2014 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
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

#include "config.h"
#include <string.h>
#include <unistd.h>
#include <openssl/md5.h>
#include "util/ASCII.hxx"
#include "sacd_metabase.h"

sacd_metabase_t::sacd_metabase_t(sacd_disc_t* sacd_disc, const char* tags_path, const char* tags_file) {
	xmldoc = nullptr;
	metabase_loaded = false;
	uint8_t md5_data[MASTER_TOC_LEN * SACD_LSN_SIZE];
	if (sacd_disc->read_blocks_raw(START_OF_MASTER_TOC, MASTER_TOC_LEN, md5_data)) {
		uint8_t md5_hash[MD5_DIGEST_LENGTH];
		MD5(md5_data, sizeof(md5_data), md5_hash);
		for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
			char hex_byte[3];
			sprintf(hex_byte, "%02X", md5_hash[i]);
			store_id += hex_byte;
		}
		if (tags_path) {
			store_path = tags_path;
			store_file = store_path;
			store_file += "/";
			store_file += store_id;
			store_file += ".xml";
			if (access(store_file.c_str(), F_OK) == 0) {
				if (tags_file && access(tags_file, F_OK) == -1) {
					FILE* s = fopen(store_file.c_str(), "rb");
					FILE* d = fopen(tags_file, "wb");
					if (s) {
						if (d) {
							char buf[64];
							size_t size;
							while ((size = fread(buf, 1, sizeof(buf), s)) > 0) {
								fwrite(buf, 1, size, d);
							}
							fclose(d);
						}
						fclose(s);
					}
				}
			}
		}
	}
	xml_file = tags_file ? tags_file : store_file;
	metabase_loaded = init_xmldoc();
}

sacd_metabase_t::~sacd_metabase_t() {
	if (xmldoc) {
		ixmlDocument_free(xmldoc);
	}
}

bool sacd_metabase_t::get_info(uint32_t track_index, const struct TagHandler& handler, void* handler_ctx) {
	if (!metabase_loaded) {
		return false;
	}
	IXML_Node* node_track = get_track_node(track_index);
	if (!node_track) {
		return false;
	}
	IXML_NodeList* list_tags = ixmlNode_getChildNodes(node_track);
	if (!list_tags) {
		return false;
	}
	for (unsigned long tag_index = 0; tag_index < ixmlNodeList_length(list_tags); tag_index++) {
		IXML_Node* node_tag = ixmlNodeList_item(list_tags, tag_index);
		if (node_tag) {
			string node_name = ixmlNode_getNodeName(node_tag);
			if (node_name == MB_TAG_META) {
				IXML_NamedNodeMap* attr_tag = ixmlNode_getAttributes(node_tag);
				if (attr_tag) {
					string tag_name;
					string tag_value;
					string xml_tag_value;
					IXML_Node* node_attr;
					node_attr = ixmlNamedNodeMap_getNamedItem(attr_tag, MB_ATT_NAME);
					if (node_attr) {
						tag_name = ixmlNode_getNodeValue(node_attr);
						//ixmlNode_free(node_attr);
					}
					node_attr = ixmlNamedNodeMap_getNamedItem(attr_tag, MB_ATT_VALUE);
					if (node_attr) {
						xml_tag_value = ixmlNode_getNodeValue(node_attr);
						xml2utf(xml_tag_value, tag_value);
						//ixmlNode_free(node_attr);
					}
					if (tag_name.length() > 0) {
						TagType tag_type = TAG_NUM_OF_ITEM_TYPES;
						for (int i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
							if (StringEqualsCaseASCII(tag_item_names[i], tag_name.c_str())) {
								tag_type = static_cast<TagType>(i);
								break;
							}
						}
						if (tag_type != TAG_NUM_OF_ITEM_TYPES) {
							tag_handler_invoke_tag(handler, handler_ctx, tag_type, tag_value.c_str());
						}
					}
					//ixmlNamedNodeMap_free(attr_tag);
				}
			}
			//ixmlNode_free(node_tag);
		}
	}
	//ixmlNodeList_free(list_tags);
	//ixmlNode_free(node_track);
	return true;
}

bool sacd_metabase_t::init_xmldoc() {
	xmldoc = ixmlLoadDocument(xml_file.c_str());
	if (!xmldoc) {
		return false;
	}
	return true;
}

IXML_Node* sacd_metabase_t::get_track_node(uint32_t track_index) {
	string track_id = to_string(track_index + 1);
	IXML_NodeList* list_track = nullptr;
	IXML_Node* node_track_id = nullptr;
	IXML_Node* node_root = ixmlNodeList_item(ixmlDocument_getElementsByTagName(xmldoc, MB_TAG_ROOT), 0);
	if (node_root) {
		IXML_NodeList* list_store = ixmlNode_getChildNodes(node_root);
		if (list_store) {
			for (unsigned long i = 0; i < ixmlNodeList_length(list_store); i++) {
				IXML_Node* node_store = ixmlNodeList_item(list_store, i);
				if (node_store) {
					IXML_NamedNodeMap* attr_store = ixmlNode_getAttributes(node_store);
					if (attr_store) {
						IXML_Node* node_attr;
						string attr_id;
						string attr_type;
						node_attr = ixmlNamedNodeMap_getNamedItem(attr_store, MB_ATT_ID);
						if (node_attr) {
							attr_id = ixmlNode_getNodeValue(node_attr);
							//ixmlNode_free(node_attr);
						}
						node_attr = ixmlNamedNodeMap_getNamedItem(attr_store, MB_ATT_TYPE);
						if (node_attr) {
							attr_type = ixmlNode_getNodeValue(node_attr);
							//ixmlNode_free(node_attr);
						}
						if (attr_id == store_id && attr_type == METABASE_TYPE) {
							list_track = ixmlNode_getChildNodes(node_store);
							break;
						}
					}
				}
			}
		}
	}
	if (list_track) {
		for (unsigned long i = 0; i < ixmlNodeList_length(list_track); i++) {
			IXML_Node* node_track = ixmlNodeList_item(list_track, i);
			if (node_track) {
				IXML_NamedNodeMap* attr_track = ixmlNode_getAttributes(node_track);
				if (attr_track) {
					IXML_Node* node_attr;
					string attr_id;
					node_attr = ixmlNamedNodeMap_getNamedItem(attr_track, MB_ATT_ID);
					if (node_attr) {
						attr_id = ixmlNode_getNodeValue(node_attr);
						//ixmlNode_free(node_attr);
					}
					if (attr_id == track_id) {
						node_track_id = node_track;
						break;
					}
				}
			}
		}
	}
	return node_track_id;
}

void sacd_metabase_t::utf2xml(string& src, string& dst) {
	dst.clear();
	for (size_t i = 0; i < src.length(); i++) {
		if (src[i] == '\r') {
			dst += "&#13;";
		}
		else if (src[i] == '\n') {
			dst += "&#10;";
		}
		else {
			dst += src[i];
		}
	}
}

void sacd_metabase_t::xml2utf(string& src, string& dst) {
	dst.clear();
	for (size_t i = 0; i < src.length(); i++) {
		if (strncmp(&src.c_str()[i], "&#13;", 5) == 0) {
			dst += '\r';
			i += 4;
		}
		else if (strncmp(&src.c_str()[i], "&#10;", 5) == 0) {
			dst += '\n';
			i += 4;
		}
		else {
			dst += src[i];
		}
	}
}
