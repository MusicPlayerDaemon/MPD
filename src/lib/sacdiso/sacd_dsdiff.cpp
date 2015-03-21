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

#include "tag/TagId3.hxx"

#ifdef ENABLE_ID3TAG
#include <id3tag.h>
#endif

#include "sacd_dsdiff.h"

#define MARK_TIME(m) ((double)m.hours * 60 * 60 + (double)m.minutes * 60 + (double)m.seconds + ((double)m.samples + (double)m.offset) / (double)samplerate)

sacd_dsdiff_t::sacd_dsdiff_t() {
	current_track = 0;
	is_emaster = false;
	is_dst_encoded = false;
}

sacd_dsdiff_t::~sacd_dsdiff_t() {
	close();
}

uint32_t sacd_dsdiff_t::get_tracks() {
	return get_tracks(track_area);
}

uint32_t sacd_dsdiff_t::get_tracks(area_id_e area_id) {
	if ((area_id == AREA_TWOCH && channel_count == 2) || (area_id == AREA_MULCH && channel_count > 2) || area_id == AREA_BOTH) {
		return track_index.size();
	}
	return 0;
}

uint32_t sacd_dsdiff_t::get_channels() {
	return channel_count;
}

uint32_t sacd_dsdiff_t::get_loudspeaker_config() {
	return loudspeaker_config;
}

uint32_t sacd_dsdiff_t::get_samplerate() {
	return samplerate;
}

uint16_t sacd_dsdiff_t::get_framerate() {
	return framerate;
}

uint64_t sacd_dsdiff_t::get_size() {
	return current_size;
}

uint64_t sacd_dsdiff_t::get_offset() {
	return sacd_media->get_position() - current_offset;
}

double sacd_dsdiff_t::get_duration() {
	return get_duration(current_track);
}

double sacd_dsdiff_t::get_duration(uint32_t _track_index) {
	if (_track_index < track_index.size()) {
		double stop_time = is_emaster ? track_index[_track_index].stop_time2 : track_index[_track_index].stop_time1;
		return stop_time - track_index[_track_index].start_time;
	}
	return 0.0;
}

bool sacd_dsdiff_t::is_dst() {
	return is_dst_encoded != 0;
}

bool sacd_dsdiff_t::open(sacd_media_t* _sacd_media) {
	sacd_media = _sacd_media;
	dsti_size = 0;
	Chunk ck;
	ID id;
	bool skip_emaster_chunks = false; // if true plays as the single track
	uint32_t start_mark_count = 0;
	id3tags_t t_old;
	track_index.resize(0);
	id3tags.resize(0);
	if (!sacd_media->seek(0)) {
		return false;
	}
	if (!(sacd_media->read(&ck, sizeof(ck)) == sizeof(ck) && ck.has_id("FRM8"))) {
		return false;
	}
	if (!(sacd_media->read(&id, sizeof(id)) == sizeof(id) && id.has_id("DSD "))) {
		return false;
	}
	frm8_size = ck.get_size();
	id3_offset = sizeof(ck) + ck.get_size();
	while ((uint64_t)sacd_media->get_position() < frm8_size + sizeof(ck)) {
		if (!(sacd_media->read(&ck, sizeof(ck)) == sizeof(ck))) {
			return false;
		}
		if (ck.has_id("FVER") && ck.get_size() == 4) {
			if (!(sacd_media->read(&version, sizeof(version)) == sizeof(version))) {
				return false;
			}
			version = hton32(version);
		}
		else if (ck.has_id("PROP")) {
			if (!(sacd_media->read(&id, sizeof(id)) == sizeof(id) && id.has_id("SND "))) {
				return false;
			}
			uint64_t id_prop_size = ck.get_size() - sizeof(id);
			uint64_t id_prop_read = 0;
			while (id_prop_read < id_prop_size) {
				if (!(sacd_media->read(&ck, sizeof(ck)) == sizeof(ck))) {
					return false;
				}
				if (ck.has_id("FS  ") && ck.get_size() == 4) {
					if (!(sacd_media->read(&samplerate, sizeof(samplerate)) == sizeof(samplerate))) {
						return false;
					}
					samplerate = hton32(samplerate);
				}
				else if (ck.has_id("CHNL")) {
					if (!(sacd_media->read(&channel_count, sizeof(channel_count)) == sizeof(channel_count))) {
						return false;
					}
					channel_count = hton16(channel_count);
					switch (channel_count) {
					case 2:
						loudspeaker_config = 0;
						break;
					case 5:
						loudspeaker_config = 3;
						break;
					case 6:
						loudspeaker_config = 4;
						break;
					default:
						loudspeaker_config = 65535;
						break;
					}
					sacd_media->skip(ck.get_size() - sizeof(channel_count));
				}
				else if (ck.has_id("CMPR")) {
					if (!(sacd_media->read(&id, sizeof(id)) == sizeof(id))) {
						return false;
					}
					if (id.has_id("DSD ")) {
						is_dst_encoded = false;
					}
					if (id.has_id("DST ")) {
						is_dst_encoded = true;
					}
					sacd_media->skip(ck.get_size() - sizeof(id));
				}
				else if (ck.has_id("LSCO")) {
					if (!(sacd_media->read(&loudspeaker_config, sizeof(loudspeaker_config)) == sizeof(loudspeaker_config))) {
						return false;
					}
					loudspeaker_config = hton16(loudspeaker_config);
					sacd_media->skip(ck.get_size() - sizeof(loudspeaker_config));
				}
				else if (ck.has_id("ID3 ")) {
					t_old.index  = 0;
					t_old.offset = sacd_media->get_position();
					t_old.size   = ck.get_size();
					t_old.data.resize((uint32_t)ck.get_size());
					sacd_media->read(t_old.data.data(), t_old.data.size());
				}
				else {
					sacd_media->skip(ck.get_size());
				}
				id_prop_read += sizeof(ck) + ck.get_size() + (ck.get_size() & 1);
				sacd_media->skip(sacd_media->get_position() & 1);
			}
		}
		else if (ck.has_id("DSD ")) {
			data_offset = sacd_media->get_position();
			data_size = ck.get_size();
			framerate = 75;
			dsd_frame_size = samplerate / 8 * channel_count / framerate;
			frame_count = (uint32_t)(data_size / dsd_frame_size);
			sacd_media->skip(ck.get_size());
			track_t s;
			s.start_time = 0.0;
			s.stop_time1 = s.stop_time2 = (double)frame_count / framerate;
			track_index.push_back(s);
		}
		else if (ck.has_id("DST ")) {
			data_offset = sacd_media->get_position();
			data_size = ck.get_size();
			if (!(sacd_media->read(&ck, sizeof(ck)) == sizeof(ck) && ck.has_id("FRTE") && ck.get_size() == 6)) {
				return false;
			}
			data_offset += sizeof(ck) + ck.get_size();
			data_size -= sizeof(ck) + ck.get_size();
			current_offset = data_offset;
			current_size = data_size;
			if (!(sacd_media->read(&frame_count, sizeof(frame_count)) == sizeof(frame_count))) {
				return false;
			}
			frame_count = hton32(frame_count);
			if (!(sacd_media->read(&framerate, sizeof(framerate)) == sizeof(framerate))) {
				return false;
			}
			framerate = hton16(framerate);
			dsd_frame_size = samplerate / 8 * channel_count / framerate;
			sacd_media->seek(data_offset + data_size);
			track_t s;
			s.start_time = 0.0;
			s.stop_time1 = s.stop_time2 = (double)frame_count / framerate;
			track_index.push_back(s);
		}
		else if (ck.has_id("DSTI")) {
			dsti_offset = sacd_media->get_position();
			dsti_size = ck.get_size();
			sacd_media->skip(ck.get_size());
		}
		else if (ck.has_id("DIIN") && !skip_emaster_chunks) {
			uint64_t id_diin_size = ck.get_size();
			uint64_t id_diin_read = 0;
			while (id_diin_read < id_diin_size) {
				if (!(sacd_media->read(&ck, sizeof(ck)) == sizeof(ck))) {
					return false;
				}
				if (ck.has_id("MARK") && ck.get_size() >= sizeof(Marker)) {
					Marker m;
					if (sacd_media->read(&m, sizeof(Marker)) == sizeof(Marker)) {
						m.hours       = hton16(m.hours);
						m.samples     = hton32(m.samples);
						m.offset      = hton32(m.offset);
						m.markType    = hton16(m.markType);
						m.markChannel = hton16(m.markChannel);
						m.TrackFlags  = hton16(m.TrackFlags);
						m.count       = hton32(m.count);
						switch (m.markType) {
						case TrackStart:
							if (start_mark_count > 0) {
								track_t s;
								track_index.push_back(s);
							}
							start_mark_count++;
							if (track_index.size() > 0) {
								track_index[track_index.size() - 1].start_time = MARK_TIME(m);
								track_index[track_index.size() - 1].stop_time2 = (double)frame_count / framerate;
								track_index[track_index.size() - 1].stop_time1 = track_index[track_index.size() - 1].stop_time2;
								if (track_index.size() - 1 > 0) {
									if (track_index[track_index.size() - 2].stop_time2 > track_index[track_index.size() - 1].start_time) {
										track_index[track_index.size() - 2].stop_time2 = track_index[track_index.size() - 1].start_time;
										track_index[track_index.size() - 2].stop_time1 = track_index[track_index.size() - 2].stop_time2;
									}
								}
							}
							break;
						case TrackStop:
							if (track_index.size() > 0) {
								track_index[track_index.size() - 1].stop_time1 = MARK_TIME(m);
							}
							break;
						}
					}
					sacd_media->skip(ck.get_size() - sizeof(Marker));
				}
				else {
					sacd_media->skip(ck.get_size());
				}
				id_diin_read += sizeof(ck) + ck.get_size();
				sacd_media->skip(sacd_media->get_position() & 1);
			}
		}
		else if (ck.has_id("ID3 ") && !skip_emaster_chunks) {
			id3_offset = min(id3_offset, (uint64_t)sacd_media->get_position() - sizeof(ck));
			id3tags_t t;
			t.index  = id3tags.size();
			t.offset = sacd_media->get_position();
			t.size   = ck.get_size();
			t.data.resize((uint32_t)ck.get_size());
			sacd_media->read(t.data.data(), t.data.size());
			id3tags.push_back(t);
		}
		else {
			sacd_media->skip(ck.get_size());
		}
		sacd_media->skip(sacd_media->get_position() & 1);
	}
	if (id3tags.size() == 0) {
		if (t_old.size > 0) {
			id3tags.push_back(t_old);
		}
	}
	sacd_media->seek(data_offset);
	set_emaster(false);
	index_id3tags();
	return track_index.size() > 0;
}

bool sacd_dsdiff_t::close() {
	current_track = 0;
	track_index.resize(0);
	id3tags.resize(0);
	dsti_size = 0;
	if (!sacd_media->seek(0)) {
		return false;
	}
	return true;
}

void sacd_dsdiff_t::select_area(area_id_e area_id) {
	track_area = area_id;
}

void sacd_dsdiff_t::set_emaster(bool emaster) {
	is_emaster = emaster;
}

bool sacd_dsdiff_t::select_track(uint32_t _track_index, area_id_e area_id, uint32_t _offset) {
	(void)area_id;
	if (_track_index < track_index.size()) {
		current_track = _track_index;
		double t0 = track_index[current_track].start_time;
		double t1 = is_emaster ? track_index[current_track].stop_time2 : track_index[current_track].stop_time1;
		uint64_t offset = (uint64_t)(t0 * framerate / frame_count * data_size) + _offset;
		uint64_t size = (uint64_t)(t1 * framerate / frame_count * data_size) - offset;
		if (is_dst_encoded) {
			if (dsti_size > 0) {
				if ((uint32_t)(t0 * framerate) < (uint32_t)(dsti_size / sizeof(DSTFrameIndex) - 1)) {
					current_offset = get_dsti_for_frame((uint32_t)(t0 * framerate));
				}
				else {
					current_offset = data_offset + offset;
				}
				if ((uint32_t)(t1 * framerate) < (uint32_t)(dsti_size / sizeof(DSTFrameIndex) - 1)) {
					current_size = get_dsti_for_frame((uint32_t)(t1 * framerate)) - current_offset;
				}
				else {
					current_size = size;
				}
			}
			else {
				current_offset = data_offset + offset;
				current_size = size;
			}
		}
		else {
			current_offset = data_offset + (offset / dsd_frame_size) * dsd_frame_size;
			current_size = (size / dsd_frame_size) * dsd_frame_size;
		}
	}
	sacd_media->seek(current_offset);
	return true;
}

bool sacd_dsdiff_t::read_frame(uint8_t* frame_data, size_t* frame_size, frame_type_e* frame_type) {
//static uint64_t s_next_frame = 0;
//if (sacd_media->get_position() != s_next_frame) {
//	console::printf("offset: %d - %d", (uint32_t)sacd_media->get_position(), (uint32_t)s_next_frame);
//}
	if (is_dst_encoded) {
		Chunk ck;
		while ((uint64_t)sacd_media->get_position() < current_offset + current_size && sacd_media->read(&ck, sizeof(ck)) == sizeof(ck)) {
			if (ck.has_id("DSTF") && ck.get_size() <= (uint64_t)*frame_size) {
				if (sacd_media->read(frame_data, (size_t)ck.get_size()) == ck.get_size()) {
					sacd_media->skip(ck.get_size() & 1);
					*frame_size = (size_t)ck.get_size();
					*frame_type = FRAME_DST;
//s_next_frame = sacd_media->get_position();
					return true;
				}
				break;
			}
			else if (ck.has_id("DSTC") && ck.get_size() == 4) {
				uint32_t crc;
				if (ck.get_size() == sizeof(crc)) {
					if (sacd_media->read(&crc, sizeof(crc)) != sizeof(crc)) {
						break;
					}
				}
				else {
					sacd_media->skip(ck.get_size());
					sacd_media->skip(ck.get_size() & 1);
				}
			}
			else {
				sacd_media->seek(sacd_media->get_position() + 1 - (int)sizeof(ck));
			}
		}
	}
	else {
		uint64_t position = sacd_media->get_position();
		*frame_size = min(*frame_size, (size_t)max((int64_t)0, (int64_t)(current_offset + current_size) - (int64_t)position));
		if (*frame_size > 0) {
			*frame_size = sacd_media->read(frame_data, *frame_size);
			*frame_size -= *frame_size % channel_count;
			if (*frame_size > 0) {
				*frame_type = FRAME_DSD;
//s_next_frame = sacd_media->get_position();
				return true;
			}
		}
	}
	*frame_type = FRAME_INVALID;
	return false;
}

bool sacd_dsdiff_t::seek(double seconds) {
	uint64_t offset = min((uint64_t)(get_size() * seconds / get_duration()), get_size());
	if (is_dst_encoded) {
		if (dsti_size > 0) {
			uint32_t frame = min((uint32_t)((track_index[current_track].start_time + seconds) * framerate), frame_count - 1);
			if (frame < (uint32_t)(dsti_size / sizeof(DSTFrameIndex) - 1)) {
				offset = get_dsti_for_frame(frame) - current_offset;
			}
		}
	}
	else {
		offset = (offset / dsd_frame_size) * dsd_frame_size;
	}
	sacd_media->seek(current_offset + offset);
	return true;
}

void sacd_dsdiff_t::get_info(uint32_t _track_index, const struct tag_handler* handler, void* handler_ctx) {
	for (uint32_t i = 0; i < id3tags.size(); i++) {
		if (_track_index == id3tags[i].index) {
			get_id3tags(_track_index, handler, handler_ctx);
			break;
		}
	}
}

uint64_t sacd_dsdiff_t::get_dsti_for_frame(uint32_t frame_nr) {
	uint64_t      cur_offset;
	DSTFrameIndex frame_index;
	cur_offset = sacd_media->get_position();
	frame_nr = min(frame_nr, (uint32_t)(dsti_size / sizeof(DSTFrameIndex) - 1));
	sacd_media->seek(dsti_offset + frame_nr * sizeof(DSTFrameIndex));
	cur_offset = sacd_media->get_position();
	sacd_media->read(&frame_index, sizeof(DSTFrameIndex));
	sacd_media->seek(cur_offset);
	return hton64(frame_index.offset) - sizeof(Chunk);
}

void sacd_dsdiff_t::get_id3tags(uint32_t _track_index, const struct tag_handler* handler, void* handler_ctx) {
#ifdef ENABLE_ID3TAG
	if (id3tags[_track_index].size > 0) {
		id3_byte_t* dsdid3 = (id3_byte_t*)&id3tags[_track_index].data[0];
		const id3_length_t count = id3tags[_track_index].size;
		struct id3_tag* id3_tag = id3_tag_parse(dsdid3, count);
		if (id3_tag != nullptr) {
			scan_id3_tag(id3_tag, handler, handler_ctx);
			id3_tag_delete(id3_tag);
		}
	}
#endif
}

void sacd_dsdiff_t::index_id3tags() {
#ifdef ENABLE_ID3TAG
	/*
	for (uint32_t i = 0; i < id3tags.size(); i++) {
		if (id3tags[i].size > 0) {
			id3_byte_t* dsdid3 = (id3_byte_t*)&id3tags[i].data[0];
			const id3_length_t count = id3tags[i].size;
			struct id3_tag* id3_tag = id3_tag_parse(dsdid3, count);
			if (id3_tag != nullptr) {
				const struct id3_frame* frame;
				frame = id3_tag_findframe(id3_tag, ID3_FRAME_TRACK, 0);
				if (frame != nullptr) {
					const id3_field* field = id3_frame_field(frame, 0);
					if (field != nullptr) {
						long tracknumber = id3_field_getint(field);
						id3tags[i].index = tracknumber;
					}
				}
				id3_tag_delete(id3_tag);
			}
		}
	}
	*/
#endif
}

