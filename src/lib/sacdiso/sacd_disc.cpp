/*
* MPD SACD Decoder plugin
* Copyright (c) 2017 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
*
* This module partially uses code from SACD Ripper http://code.google.com/p/sacd-ripper/ project
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

#include <iconv.h>
#include <string>
#include <malloc.h>
#include <string.h>
#include "sacd_disc.h"

using namespace std;

static inline int has_two_channel(scarletbook_handle_t* handle) {
	return handle->twoch_area_idx != -1;
}

static inline int has_multi_channel(scarletbook_handle_t* handle) {
	return handle->mulch_area_idx != -1;
}

static inline int has_both_channels(scarletbook_handle_t* handle) {
	return handle->twoch_area_idx != -1 && handle->mulch_area_idx != -1;
}

static inline area_toc_t* get_two_channel(scarletbook_handle_t* handle) {
	return handle->twoch_area_idx == -1 ? 0 : handle->area[handle->twoch_area_idx].area_toc;
}

static inline area_toc_t* get_multi_channel(scarletbook_handle_t* handle) {
	return handle->mulch_area_idx == -1 ? 0 : handle->area[handle->mulch_area_idx].area_toc;
}

typedef struct {
	uint32_t    id;
	const char* name;
} codepage_id_t;

static codepage_id_t codepage_ids[] = {
	{CP_ACP, character_set[0]},
	{20127,  character_set[1]},
	{28591,  character_set[2]},
	{932,    character_set[3]},
	{949,    character_set[4]},
	{936,    character_set[5]},
	{950,    character_set[6]},
	{28591,  character_set[7]},
};

static inline string charset_convert(const char* instring, size_t insize, uint8_t codepage_index) {
	string utf8_string;
	if (codepage_index < sizeof(codepage_ids) / sizeof(*codepage_ids)) {
		const char* codepage_name = codepage_ids[codepage_index].name;
		iconv_t conv = iconv_open("UTF-8", codepage_name);
		if (conv != (iconv_t)-1) {
			size_t utf8_size = 3 * insize;
			utf8_string.resize(utf8_size);
			const char* inbuf = instring;
			size_t inbytesleft = insize;
			const char* outbuf = utf8_string.data();
			size_t outbytesleft = utf8_string.size();
			if (iconv(conv, const_cast<char**>(&inbuf), &inbytesleft, const_cast<char**>(&outbuf), &outbytesleft) != (size_t)-1) {
				utf8_string.resize(utf8_size - outbytesleft);
				return utf8_string;
			}
			iconv_close(conv);
		}
	}
	utf8_string = instring;
	return utf8_string;
}

static inline int get_channel_count(audio_frame_info_t* frame_info) {
  if (frame_info->channel_bit_2 == 1 && frame_info->channel_bit_3 == 0) {
		return 6;
  }
  else if (frame_info->channel_bit_2 == 0 && frame_info->channel_bit_3 == 1) {
		return 5;
  }
  else {
		return 2;
  }
}

sacd_disc_t::sacd_disc_t() {
	sacd_media = nullptr;
	sb_handle.master_data = nullptr;
	sb_handle.twoch_area_idx = -1;
	sb_handle.mulch_area_idx = -1;
	sb_handle.area_count = 0;
	track_area = AREA_BOTH;
}

sacd_disc_t::~sacd_disc_t() {
}

scarletbook_handle_t* sacd_disc_t::get_handle() {
	return &sb_handle;
}

scarletbook_area_t* sacd_disc_t::get_area(area_id_e area_id) {
	switch (area_id) {
	case AREA_TWOCH:
		if (sb_handle.twoch_area_idx != -1) {
			return &sb_handle.area[sb_handle.twoch_area_idx];
		}
		break;
	case AREA_MULCH:
		if (sb_handle.mulch_area_idx != -1) {
			return &sb_handle.area[sb_handle.mulch_area_idx];
		}
		break;
	default:
		break;
	}
	return nullptr;
}

uint32_t sacd_disc_t::get_tracks() {
	return get_tracks(track_area);
}

uint32_t sacd_disc_t::get_tracks(area_id_e area_id) {
	scarletbook_area_t* area = get_area(area_id);
	if (area) {
		return get_area(area_id)->area_toc->track_count;
	}
	return 0;
}

area_id_e sacd_disc_t::get_track_area_id() {
	return track_area;
}

uint32_t sacd_disc_t::get_track_index() {
	return sel_track_index;
}

uint32_t sacd_disc_t::get_track_length_lsn() {
	return sel_track_length_lsn;
}

uint32_t sacd_disc_t::get_channels() {
	return get_area(track_area) ? get_area(track_area)->area_toc->channel_count : 0;
}

uint32_t sacd_disc_t::get_loudspeaker_config() {
	return get_area(track_area) ? get_area(track_area)->area_toc->loudspeaker_config : 0;
}

uint32_t sacd_disc_t::get_samplerate() {
	return SACD_SAMPLING_FREQUENCY;
}

uint16_t sacd_disc_t::get_framerate() {
	return 75;
}

uint64_t sacd_disc_t::get_size() {
	return (uint64_t)sel_track_length_lsn * (uint64_t)sector_size;
}

uint64_t sacd_disc_t::get_offset() {
	return (uint64_t)sel_track_current_lsn * (uint64_t)sector_size;
}

double sacd_disc_t::get_duration() {
	return get_duration(sel_track_index);
}

double sacd_disc_t::get_duration(uint32_t track_index) {
	scarletbook_area_t* area = get_area(track_area);
	double track_duration = 0.0;
	if (area) {
		if (track_index < area->area_toc->track_count) {
			area_tracklist_time_duration_t duration = area->area_tracklist_time->duration[track_index];
			track_duration = duration.minutes * 60.0 + duration.seconds * 1.0 + duration.frames / 75.0;
		}
	}
	return track_duration;
}

void sacd_disc_t::get_info(uint32_t track_index, const struct TagHandler& handler, void *handler_ctx) {
	scarletbook_area_t* area = get_area(track_area);
	if (!(area != nullptr && track_index < get_tracks(track_area))) {
		return;
	}
	string tag_value;
	if (get_handle()->master_toc->album_set_size > 1) {
		if (get_handle()->master_toc->album_sequence_number > 0) {
			tag_value = to_string(get_handle()->master_toc->album_sequence_number);
			tag_handler_invoke_tag(handler, handler_ctx, TAG_DISC, tag_value.c_str());
		}
	}
	scarletbook_handle_t* sb = get_handle();
	if (sb->master_toc->disc_date_year > 0) {
		tag_value = to_string(sb->master_toc->disc_date_year);
		/*
		if (sb->master_toc->disc_date_month > 0) {
			tag_value += "-";
			if (sb->master_toc->disc_date_month < 10) {
				tag_value += "0";
			}
			tag_value += to_string(sb->master_toc->disc_date_month);
			if (sb->master_toc->disc_date_day > 0) {
				tag_value += "-";
				if (sb->master_toc->disc_date_day < 10) {
					tag_value += "0";
				}
				tag_value += to_string(sb->master_toc->disc_date_day);
			}
		}
		*/
		tag_handler_invoke_tag(handler, handler_ctx, TAG_DATE, tag_value.c_str());
	}
	if (!sb->master_text.album_title.empty()) {
		tag_value  = sb->master_text.album_title;
		tag_value += " (";
		tag_value += (get_track_area_id() == AREA_TWOCH) ? "2CH" : "MCH";
		tag_value += "-";
		tag_value += is_dst() ? "DST" : "DSD";
		tag_value += ")";
		tag_handler_invoke_tag(handler, handler_ctx, TAG_ALBUM, tag_value.c_str());
	}
	if (!sb->master_text.album_artist.empty()) {
		tag_handler_invoke_tag(handler, handler_ctx, TAG_ARTIST, sb->master_text.album_artist.c_str());
	}
	if (!area->area_track_text[track_index].track_type_title.empty()) {
		char track_number_string[4];
		sprintf(track_number_string, "%02d", track_index + 1);
		tag_value  = (get_track_area_id() == AREA_TWOCH) ? "2CH" : "MCH";
		tag_value += " - ";
		tag_value += track_number_string;
		tag_value += " - ";
		tag_value += area->area_track_text[track_index].track_type_title;
		tag_handler_invoke_tag(handler, handler_ctx, TAG_TITLE, tag_value.c_str());
	}
	if (!area->area_track_text[track_index].track_type_composer.empty()) {
		tag_handler_invoke_tag(handler, handler_ctx, TAG_COMPOSER, area->area_track_text[track_index].track_type_composer.c_str());
	}
	if (!area->area_track_text[track_index].track_type_performer.empty()) {
		tag_handler_invoke_tag(handler, handler_ctx, TAG_PERFORMER, area->area_track_text[track_index].track_type_performer.c_str());
	}
	if (!area->area_track_text[track_index].track_type_message.empty()) {
		tag_handler_invoke_tag(handler, handler_ctx, TAG_COMMENT, area->area_track_text[track_index].track_type_message.c_str());
	}
	if (area->area_isrc_genre) {
		if (area->area_isrc_genre->track_genre[track_index].category == 1) {
			uint8_t genre = area->area_isrc_genre->track_genre[track_index].genre;
			if (genre > 0) {
				tag_handler_invoke_tag(handler, handler_ctx, TAG_GENRE, album_genre[genre]);
			}
		}
	}
}

bool sacd_disc_t::is_dst() {
	return is_dst_encoded;
}

void sacd_disc_t::set_emaster(bool emaster) {
	is_emaster = emaster;
}

bool sacd_disc_t::open(sacd_media_t* _sacd_media, open_mode_e _mode) {
	sacd_media = _sacd_media;
	mode = _mode;
	sb_handle.master_data = nullptr;
	sb_handle.area_count = 0;
	sb_handle.twoch_area_idx = -1;
	sb_handle.mulch_area_idx = -1;
	sb_handle.area[0].area_data = nullptr;
	sb_handle.area[1].area_data = nullptr;
	char sacdmtoc[8];
	sector_size = 0;
	sector_bad_reads = 0;
	if (!sacd_media->seek((uint64_t)START_OF_MASTER_TOC * (uint64_t)SACD_LSN_SIZE)) {
		return false;
	}
	if (sacd_media->read(sacdmtoc, 8) == 8) {
		if (memcmp(sacdmtoc, "SACDMTOC", 8) == 0) {
			sector_size = SACD_LSN_SIZE;
			buffer = sector_buffer;
		}
	}
	if (!sacd_media->seek((uint64_t)START_OF_MASTER_TOC * (uint64_t)SACD_PSN_SIZE + 12)) {
		return false;
	}
	if (sacd_media->read(sacdmtoc, 8) == 8) {
		if (memcmp(sacdmtoc, "SACDMTOC", 8) == 0) {
			sector_size = SACD_PSN_SIZE;
			buffer = sector_buffer + 12;
		}
	}
	if (!sacd_media->seek(0)) {
		return false;
	}
	if (sector_size == 0) {
		return false;
	}
	if (!read_master_toc()) {
		close();
		return false;
	}
	if (sb_handle.master_toc->area_1_toc_1_start) {
		if (sb_handle.area[sb_handle.area_count].area_data) {
			free(sb_handle.area[sb_handle.area_count].area_data);
			sb_handle.area[sb_handle.area_count].area_data = 0;
		}
		sb_handle.area[sb_handle.area_count].area_data = (uint8_t*)malloc(sb_handle.master_toc->area_1_toc_size * SACD_LSN_SIZE);
		if (!sb_handle.area[sb_handle.area_count].area_data) {
			close();
			return false;
		}
		if (!read_blocks_raw(sb_handle.master_toc->area_1_toc_1_start, sb_handle.master_toc->area_1_toc_size, sb_handle.area[sb_handle.area_count].area_data)) {
			sb_handle.master_toc->area_1_toc_1_start = 0;
		}
		else {
			if (read_area_toc(sb_handle.area_count)) {
				sb_handle.area_count++;
			}
		}
	}
	if (sb_handle.master_toc->area_2_toc_1_start) {
		if (sb_handle.area[sb_handle.area_count].area_data) {
			free(sb_handle.area[sb_handle.area_count].area_data);
			sb_handle.area[sb_handle.area_count].area_data = 0;
		}
		sb_handle.area[sb_handle.area_count].area_data = (uint8_t*)malloc(sb_handle.master_toc->area_2_toc_size * SACD_LSN_SIZE);
		if (!sb_handle.area[sb_handle.area_count].area_data) {
			close();
			return false;
		}
		if (!read_blocks_raw(sb_handle.master_toc->area_2_toc_1_start, sb_handle.master_toc->area_2_toc_size, sb_handle.area[sb_handle.area_count].area_data)) {
			sb_handle.master_toc->area_2_toc_1_start = 0;
			close();
			return true;
		}
		if (read_area_toc(sb_handle.area_count)) {
			sb_handle.area_count++;
		}
	}
	return true;
}

bool sacd_disc_t::close() {
	if (has_two_channel(&sb_handle)) {
		free_area(&sb_handle.area[sb_handle.twoch_area_idx]);
		if (sb_handle.area[sb_handle.twoch_area_idx].area_data) {
			free(sb_handle.area[sb_handle.twoch_area_idx].area_data);
			sb_handle.area[sb_handle.twoch_area_idx].area_data = nullptr;
		}
		sb_handle.twoch_area_idx = -1;
	}
	if (has_multi_channel(&sb_handle))	{
		free_area(&sb_handle.area[sb_handle.mulch_area_idx]);
		if (sb_handle.area[sb_handle.mulch_area_idx].area_data) {
			free(sb_handle.area[sb_handle.mulch_area_idx].area_data);
			sb_handle.area[sb_handle.mulch_area_idx].area_data = nullptr;
		}
		sb_handle.mulch_area_idx = -1;
	}
	sb_handle.area_count = 0;
	if (sb_handle.master_data) {
		free(sb_handle.master_data);
		sb_handle.master_data = nullptr;
	}
	return true;
}

void sacd_disc_t::select_area(area_id_e area_id) {
	track_area = area_id;
	is_dst_encoded = get_area(area_id) ? get_area(area_id)->area_toc->frame_format == FRAME_FORMAT_DST : false;
}

bool sacd_disc_t::select_track(uint32_t track_index, area_id_e area_id, uint32_t offset) {
	scarletbook_area_t* area = get_area(area_id);
	if (area != nullptr && track_index < get_tracks(area_id)) {
		sel_track_index = track_index;
		track_area = area_id;
		if (!is_emaster) {
			sel_track_start_lsn = area->area_tracklist_offset->track_start_lsn[track_index];
			sel_track_length_lsn = area->area_tracklist_offset->track_length_lsn[track_index];
		}
		else {
			if (track_index > 0) {
				sel_track_start_lsn = area->area_tracklist_offset->track_start_lsn[track_index];
			}
			else {
				sel_track_start_lsn = area->area_toc->track_start;
			}
			if (track_index < get_tracks(area_id) - 1) {
				sel_track_length_lsn = area->area_tracklist_offset->track_start_lsn[track_index + 1] - sel_track_start_lsn + 1;
			}
			else {
				sel_track_length_lsn = area->area_toc->track_end - sel_track_start_lsn;
			}
		}
		sel_track_current_lsn = sel_track_start_lsn + offset;
		channel_count = area->area_toc->channel_count;
		memset(&audio_sector, 0, sizeof(audio_sector));
		memset(&frame, 0, sizeof(frame));
		packet_info_idx = 0;
		sacd_media->seek((uint64_t)sel_track_current_lsn * (uint64_t)sector_size);
		return true;
	}
	return false;
}

bool sacd_disc_t::read_frame(uint8_t* frame_data, size_t* frame_size, frame_type_e* frame_type) {
	sector_bad_reads = 0;
	while (sel_track_current_lsn < sel_track_start_lsn + sel_track_length_lsn) {
		if (sector_bad_reads > 0) {
			buffer_offset = 0;
			packet_info_idx = 0;
			memset(&audio_sector, 0, sizeof(audio_sector));
			memset(&frame, 0, sizeof(frame));
			*frame_type = FRAME_INVALID;
			return true;
		}
		if (packet_info_idx == audio_sector.header.packet_info_count) {
			// obtain the next sector data block
			buffer_offset = 0;
			packet_info_idx = 0;
			size_t read_bytes = sacd_media->read(sector_buffer, sector_size);
			sel_track_current_lsn++;
			if (read_bytes != sector_size) {
				sector_bad_reads++;
				continue;
			}
			memcpy(&audio_sector.header, buffer + buffer_offset, AUDIO_SECTOR_HEADER_SIZE);
			buffer_offset += AUDIO_SECTOR_HEADER_SIZE;
			for (uint8_t i = 0; i < audio_sector.header.packet_info_count; i++) {
				audio_sector.packet[i].frame_start = ((buffer + buffer_offset)[0] >> 7) & 1;
				audio_sector.packet[i].data_type = ((buffer + buffer_offset)[0] >> 3) & 7;
				audio_sector.packet[i].packet_length = ((buffer + buffer_offset)[0] & 7) << 8 | (buffer + buffer_offset)[1];
				buffer_offset += AUDIO_PACKET_INFO_SIZE;
			}
			if (audio_sector.header.dst_encoded) {
				memcpy(audio_sector.frame, buffer + buffer_offset, AUDIO_FRAME_INFO_SIZE * audio_sector.header.frame_info_count);
				buffer_offset += AUDIO_FRAME_INFO_SIZE * audio_sector.header.frame_info_count;
			}
			else {
				for (uint8_t i = 0; i < audio_sector.header.frame_info_count; i++) {
					memcpy(&audio_sector.frame[i], buffer + buffer_offset, AUDIO_FRAME_INFO_SIZE - 1);
					buffer_offset += AUDIO_FRAME_INFO_SIZE - 1;
				}
			}
		}
		while (packet_info_idx < audio_sector.header.packet_info_count && sector_bad_reads == 0) {
			audio_packet_info_t* packet = &audio_sector.packet[packet_info_idx];
			switch (packet->data_type) {
			case DATA_TYPE_AUDIO:
				if (frame.started) {
					if (packet->frame_start) {
						if ((size_t)frame.size <= *frame_size) {
							memcpy(frame_data, frame.data, frame.size);
							*frame_size = frame.size;
						}
						else {
							sector_bad_reads++;
							continue;
						}
						*frame_type = sector_bad_reads > 0 ? FRAME_INVALID : frame.dst_encoded ? FRAME_DST : FRAME_DSD;
						frame.started = false;
						return true;
					}
				}
				else {
					if (packet->frame_start) {
						frame.size = 0;
						frame.dst_encoded = audio_sector.header.dst_encoded;
						frame.started = true;
					}
				}
				if (frame.started) {
					if ((size_t)frame.size + packet->packet_length <= *frame_size && buffer_offset + packet->packet_length <= SACD_LSN_SIZE) {
						memcpy(frame.data + frame.size, buffer + buffer_offset, packet->packet_length);
						frame.size += packet->packet_length;
					}
					else {
						sector_bad_reads++;
						continue;
					}
				}
				break;
			case DATA_TYPE_SUPPLEMENTARY:
			case DATA_TYPE_PADDING:
				break;
			default:
				break;
			}
			buffer_offset += packet->packet_length;
			packet_info_idx++;
		}
	}
	if (frame.started) {
		if ((size_t)frame.size <= *frame_size) {
			memcpy(frame_data, frame.data, frame.size);
			*frame_size = frame.size;
		}
		else {
			sector_bad_reads++;
			buffer_offset = 0;
			packet_info_idx = 0;
			memset(&audio_sector, 0, sizeof(audio_sector));
			memset(&frame, 0, sizeof(frame));
		}
		frame.started = false;
		*frame_type = sector_bad_reads > 0 ? FRAME_INVALID : frame.dst_encoded ? FRAME_DST : FRAME_DSD;
		return true;
	}
	*frame_type = FRAME_INVALID;
	return false;
}

bool sacd_disc_t::seek(double seconds) {
	uint64_t offset = (uint64_t)(get_size() * seconds / get_duration());
	return select_track(get_track_index(), get_track_area_id(), (uint32_t)(offset / sector_size));
}

bool sacd_disc_t::read_blocks_raw(uint32_t lb_start, uint32_t block_count, uint8_t* data) {
	switch (sector_size) {
	case SACD_LSN_SIZE: 
		sacd_media->seek((uint64_t)lb_start * (uint64_t)SACD_LSN_SIZE);
		if (sacd_media->read(data, block_count * SACD_LSN_SIZE) != block_count * SACD_LSN_SIZE) {
			sector_bad_reads++;
			return false;
		}
		break;
	case SACD_PSN_SIZE: 
		for (uint32_t i = 0; i < block_count; i++) {
			sacd_media->seek((uint64_t)(lb_start + i) * (uint64_t)SACD_PSN_SIZE + 12);
			if (sacd_media->read(data + i * SACD_LSN_SIZE, SACD_LSN_SIZE) != SACD_LSN_SIZE) {
				sector_bad_reads++;
				return false;
			}
		}
		break;
	}
	return true;
}

bool sacd_disc_t::read_master_toc() {
	uint8_t*      p;
	master_toc_t* master_toc;

	sb_handle.master_data = (uint8_t*)malloc(MASTER_TOC_LEN * SACD_LSN_SIZE);
	if (!sb_handle.master_data) {
		return false;
	}

	if (!read_blocks_raw(START_OF_MASTER_TOC, MASTER_TOC_LEN, sb_handle.master_data)) {
		return false;
	}

	master_toc = sb_handle.master_toc = (master_toc_t*)sb_handle.master_data;
	if (strncmp("SACDMTOC", master_toc->id, 8) != 0) {
		return false;
	}

	SWAP16(master_toc->album_set_size);
	SWAP16(master_toc->album_sequence_number);
	SWAP32(master_toc->area_1_toc_1_start);
	SWAP32(master_toc->area_1_toc_2_start);
	SWAP16(master_toc->area_1_toc_size);
	SWAP32(master_toc->area_2_toc_1_start);
	SWAP32(master_toc->area_2_toc_2_start);
	SWAP16(master_toc->area_2_toc_size);
	SWAP16(master_toc->disc_date_year);

	if (master_toc->version.major > SUPPORTED_VERSION_MAJOR || master_toc->version.minor > SUPPORTED_VERSION_MINOR) {
		return false;
	}

	// point to eof master header
	p = sb_handle.master_data + SACD_LSN_SIZE;

	// set pointers to text content
	for (int i = 0; i < MAX_LANGUAGE_COUNT; i++) {
		master_sacd_text_t* master_text = (master_sacd_text_t*)p;

		if (strncmp("SACDText", master_text->id, 8) != 0) {
			return false;
		}

		SWAP16(master_text->album_title_position);
		SWAP16(master_text->album_artist_position);
		SWAP16(master_text->album_publisher_position);
		SWAP16(master_text->album_copyright_position);
		SWAP16(master_text->album_title_phonetic_position);
		SWAP16(master_text->album_artist_phonetic_position);
		SWAP16(master_text->album_publisher_phonetic_position);
		SWAP16(master_text->album_copyright_phonetic_position);
		SWAP16(master_text->disc_title_position);
		SWAP16(master_text->disc_artist_position);
		SWAP16(master_text->disc_publisher_position);
		SWAP16(master_text->disc_copyright_position);
		SWAP16(master_text->disc_title_phonetic_position);
		SWAP16(master_text->disc_artist_phonetic_position);
		SWAP16(master_text->disc_publisher_phonetic_position);
		SWAP16(master_text->disc_copyright_phonetic_position);

		// we only use the first SACDText entry
		if (i == 0) {
			uint8_t current_charset = sb_handle.master_toc->locales[i].character_set & 0x07;

			if (master_text->album_title_position)
				sb_handle.master_text.album_title = charset_convert((char*)master_text + master_text->album_title_position, strlen((char*)master_text + master_text->album_title_position), current_charset);
			if (master_text->album_title_phonetic_position)
				sb_handle.master_text.album_title_phonetic = charset_convert((char*)master_text + master_text->album_title_phonetic_position, strlen((char*)master_text + master_text->album_title_phonetic_position), current_charset);
			if (master_text->album_artist_position)
				sb_handle.master_text.album_artist = charset_convert((char*)master_text + master_text->album_artist_position, strlen((char*)master_text + master_text->album_artist_position), current_charset);
			if (master_text->album_artist_phonetic_position)
				sb_handle.master_text.album_artist_phonetic = charset_convert((char*)master_text + master_text->album_artist_phonetic_position, strlen((char*)master_text + master_text->album_artist_phonetic_position), current_charset);
			if (master_text->album_publisher_position)
				sb_handle.master_text.album_publisher = charset_convert((char*)master_text + master_text->album_publisher_position, strlen((char*)master_text + master_text->album_publisher_position), current_charset);
			if (master_text->album_publisher_phonetic_position)
				sb_handle.master_text.album_publisher_phonetic = charset_convert((char*)master_text + master_text->album_publisher_phonetic_position, strlen((char*)master_text + master_text->album_publisher_phonetic_position), current_charset);
			if (master_text->album_copyright_position)
				sb_handle.master_text.album_copyright = charset_convert((char*)master_text + master_text->album_copyright_position, strlen((char*)master_text + master_text->album_copyright_position), current_charset);
			if (master_text->album_copyright_phonetic_position)
				sb_handle.master_text.album_copyright_phonetic = charset_convert((char*)master_text + master_text->album_copyright_phonetic_position, strlen((char*)master_text + master_text->album_copyright_phonetic_position), current_charset);

			if (master_text->disc_title_position)
				sb_handle.master_text.disc_title = charset_convert((char*)master_text + master_text->disc_title_position, strlen((char*)master_text + master_text->disc_title_position), current_charset);
			if (master_text->disc_title_phonetic_position)
				sb_handle.master_text.disc_title_phonetic = charset_convert((char*)master_text + master_text->disc_title_phonetic_position, strlen((char*)master_text + master_text->disc_title_phonetic_position), current_charset);
			if (master_text->disc_artist_position)
				sb_handle.master_text.disc_artist = charset_convert((char*)master_text + master_text->disc_artist_position, strlen((char*)master_text + master_text->disc_artist_position), current_charset);
			if (master_text->disc_artist_phonetic_position)
				sb_handle.master_text.disc_artist_phonetic = charset_convert((char*)master_text + master_text->disc_artist_phonetic_position, strlen((char*)master_text + master_text->disc_artist_phonetic_position), current_charset);
			if (master_text->disc_publisher_position)
				sb_handle.master_text.disc_publisher = charset_convert((char*)master_text + master_text->disc_publisher_position, strlen((char*)master_text + master_text->disc_publisher_position), current_charset);
			if (master_text->disc_publisher_phonetic_position)
				sb_handle.master_text.disc_publisher_phonetic = charset_convert((char*)master_text + master_text->disc_publisher_phonetic_position, strlen((char*)master_text + master_text->disc_publisher_phonetic_position), current_charset);
			if (master_text->disc_copyright_position)
				sb_handle.master_text.disc_copyright = charset_convert((char*)master_text + master_text->disc_copyright_position, strlen((char*)master_text + master_text->disc_copyright_position), current_charset);
			if (master_text->disc_copyright_phonetic_position)
				sb_handle.master_text.disc_copyright_phonetic = charset_convert((char*)master_text + master_text->disc_copyright_phonetic_position, strlen((char*)master_text + master_text->disc_copyright_phonetic_position), current_charset);
		}
		p += SACD_LSN_SIZE;
	}

	sb_handle.master_man = (master_man_t*)p;
	if (strncmp("SACD_Man", sb_handle.master_man->id, 8) != 0) {
		return false;
	}
	return true;
}

bool sacd_disc_t::read_area_toc(int area_idx) {
	area_toc_t*         area_toc;
	uint8_t*            area_data;
	uint8_t*            p;
	int                 sacd_text_idx = 0;
	scarletbook_area_t* area = &sb_handle.area[area_idx];
	uint8_t             current_charset;

	p = area_data = area->area_data;
	area_toc = area->area_toc = (area_toc_t*)area_data;

	if (strncmp("TWOCHTOC", area_toc->id, 8) != 0 && strncmp("MULCHTOC", area_toc->id, 8) != 0) {
		return false;
	}

	SWAP16(area_toc->size);
	SWAP32(area_toc->track_start);
	SWAP32(area_toc->track_end);
	SWAP16(area_toc->area_description_offset);
	SWAP16(area_toc->copyright_offset);
	SWAP16(area_toc->area_description_phonetic_offset);
	SWAP16(area_toc->copyright_phonetic_offset);
	SWAP32(area_toc->max_byte_rate);
	SWAP16(area_toc->track_text_offset);
	SWAP16(area_toc->index_list_offset);
	SWAP16(area_toc->access_list_offset);

	current_charset = area->area_toc->languages[sacd_text_idx].character_set & 0x07;

	if (area_toc->copyright_offset)
		area->copyright = charset_convert((char*)area_toc + area_toc->copyright_offset, strlen((char*)area_toc + area_toc->copyright_offset), current_charset);
	if (area_toc->copyright_phonetic_offset)
		area->copyright_phonetic = charset_convert((char*)area_toc + area_toc->copyright_phonetic_offset, strlen((char*)area_toc + area_toc->copyright_phonetic_offset), current_charset);
	if (area_toc->area_description_offset)
		area->description = charset_convert((char*)area_toc + area_toc->area_description_offset, strlen((char*)area_toc + area_toc->area_description_offset), current_charset);
	if (area_toc->area_description_phonetic_offset)
		area->description_phonetic = charset_convert((char*)area_toc + area_toc->area_description_phonetic_offset, strlen((char*)area_toc + area_toc->area_description_phonetic_offset), current_charset);

	if (area_toc->version.major > SUPPORTED_VERSION_MAJOR || area_toc->version.minor > SUPPORTED_VERSION_MINOR) {
		return false;
	}
	
	// is this the 2 channel?
	if (area_toc->channel_count == 2 && area_toc->loudspeaker_config == 0) {
		sb_handle.twoch_area_idx = area_idx;
	}
	else {
		sb_handle.mulch_area_idx = area_idx;
	}

	// Area TOC size is SACD_LSN_SIZE
	p += SACD_LSN_SIZE;

	while (p < (area_data + area_toc->size * SACD_LSN_SIZE)) {
		if (strncmp((char*)p, "SACDTTxt", 8) == 0) {
			// we discard all other SACDTTxt entries
			if (sacd_text_idx == 0) {
				for (uint8_t i = 0; i < area_toc->track_count; i++) {
					area_text_t* area_text;
					uint8_t      track_type, track_amount;
					char*        track_ptr;
					area_text = area->area_text = (area_text_t*)p;
					SWAP16(area_text->track_text_position[i]);
					if (area_text->track_text_position[i] > 0) {
						track_ptr = (char*)(p + area_text->track_text_position[i]);
						track_amount = *track_ptr;
						track_ptr += 4;
						for (uint8_t j = 0; j < track_amount; j++) {
							track_type = *track_ptr;
							track_ptr++;
							track_ptr++;                         // skip unknown 0x20
							if (*track_ptr != 0) {
								switch (track_type) {
								case TRACK_TYPE_TITLE:
									area->area_track_text[i].track_type_title = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								case TRACK_TYPE_PERFORMER:
									area->area_track_text[i].track_type_performer = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								case TRACK_TYPE_SONGWRITER:
									area->area_track_text[i].track_type_songwriter = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								case TRACK_TYPE_COMPOSER:
									area->area_track_text[i].track_type_composer = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								case TRACK_TYPE_ARRANGER:
									area->area_track_text[i].track_type_arranger = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								case TRACK_TYPE_MESSAGE:
									area->area_track_text[i].track_type_message = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								case TRACK_TYPE_EXTRA_MESSAGE:
									area->area_track_text[i].track_type_extra_message = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								case TRACK_TYPE_TITLE_PHONETIC:
									area->area_track_text[i].track_type_title_phonetic = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								case TRACK_TYPE_PERFORMER_PHONETIC:
									area->area_track_text[i].track_type_performer_phonetic = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								case TRACK_TYPE_SONGWRITER_PHONETIC:
									area->area_track_text[i].track_type_songwriter_phonetic = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								case TRACK_TYPE_COMPOSER_PHONETIC:
									area->area_track_text[i].track_type_composer_phonetic = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								case TRACK_TYPE_ARRANGER_PHONETIC:
									area->area_track_text[i].track_type_arranger_phonetic = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								case TRACK_TYPE_MESSAGE_PHONETIC:
									area->area_track_text[i].track_type_message_phonetic = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								case TRACK_TYPE_EXTRA_MESSAGE_PHONETIC:
									area->area_track_text[i].track_type_extra_message_phonetic = charset_convert(track_ptr, strlen(track_ptr), current_charset);
									break;
								}
							}
							if (j < track_amount - 1) {
								while (*track_ptr != 0) {
									track_ptr++;
								}
								while (*track_ptr == 0) {
									track_ptr++;
								}
							}
						}
					}
				}
			}
			sacd_text_idx++;
			p += SACD_LSN_SIZE;
		}
		else if (strncmp((char*)p, "SACD_IGL", 8) == 0) {
			area->area_isrc_genre = (area_isrc_genre_t*)p;
			p += SACD_LSN_SIZE * 2;
		}
		else if (strncmp((char*)p, "SACD_ACC", 8) == 0) {
			// skip
			p += SACD_LSN_SIZE * 32;
		}
		else if (strncmp((char*)p, "SACDTRL1", 8) == 0) {
			area_tracklist_offset_t* tracklist;
			tracklist = area->area_tracklist_offset = (area_tracklist_offset_t*)p;
			for (uint8_t i = 0; i < area_toc->track_count; i++) {
				SWAP32(tracklist->track_start_lsn[i]);
				SWAP32(tracklist->track_length_lsn[i]);
			}
			p += SACD_LSN_SIZE;
		}
		else if (strncmp((char*)p, "SACDTRL2", 8) == 0) {
			area->area_tracklist_time = (area_tracklist_time_t*)p;
			p += SACD_LSN_SIZE;
		}
		else {
			break;
		}
	}
	return true;
}

void sacd_disc_t::free_area(scarletbook_area_t* area) {
	for (uint8_t i = 0; i < area->area_toc->track_count; i++) {
	}
}
