/*
* DVD-Audio Decoder plugin
* Copyright (c) 2009 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
*
* DVD-Audio Decoder is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* DVD-Audio Decoder is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <math.h>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include "b2n.h"
#include "dvda_zone.h"
#include "audio_stream.h"

aob_object_t::aob_object_t(dvd_type_e type) : dvda_object_t(type) {
}

aob_object_t::~aob_object_t() {
}

double aob_object_t::get_time() {
	return PTS_TO_SEC(get_length_pts());
}


dvda_sector_pointer_t::dvda_sector_pointer_t(dvda_track_t* _track, ats_track_sector_t* p_ats_track_sector, int _index) : aob_object_t(DVDTypeSectorPointer) {
	track = _track;
	index = _index;
	first = p_ats_track_sector->first;
	last  = p_ats_track_sector->last;
}

dvda_sector_pointer_t::~dvda_sector_pointer_t() {
}

uint32_t dvda_sector_pointer_t::get_index() {
	return index;
}

uint32_t dvda_sector_pointer_t::get_length_pts() {
	uint32_t denom = track->get_last() - track->get_first() + 1;
	if (denom) {
		double pts = (double)track->get_length_pts() * (double)(last - first + 1) / (double)denom;
		return (uint32_t)pts;
	}
	return 0;
}

uint32_t dvda_sector_pointer_t::get_first() {
	return first;
}
uint32_t dvda_sector_pointer_t::get_last() {
	return last;
}


dvda_track_t::dvda_track_t(ats_track_timestamp_t* p_ats_track_timestamp, int _track) : aob_object_t(DVDTypeTrack) {
	track          = _track;
	index          = p_ats_track_timestamp->n;
	first_pts      = p_ats_track_timestamp->first_pts;
	length_pts     = p_ats_track_timestamp->len_in_pts;
	downmix_matrix = p_ats_track_timestamp->downmix_matrix < DOWNMIX_MATRICES ? p_ats_track_timestamp->downmix_matrix : -1;
}

dvda_track_t::~dvda_track_t() {
	sector_pointers.clear();
}

int dvda_track_t::sector_pointer_count() {
	return sector_pointers.size();
}

dvda_sector_pointer_t& dvda_track_t::get_sector_pointer(int sector_pointer_index) {
	return sector_pointers[sector_pointer_index];
}

void dvda_track_t::append(dvda_sector_pointer_t& dvda_sector_pointer) {
	sector_pointers.push_back(dvda_sector_pointer);
}

uint32_t dvda_track_t::get_index() {
	return index;
}

int dvda_track_t::get_track() {
	return track;
}

int dvda_track_t::get_downmix_matrix() {
	return downmix_matrix;
}

uint32_t dvda_track_t::get_length_pts() {
	return length_pts;
}

uint32_t dvda_track_t::get_first() {
	uint32_t sector = (sector_pointer_count() > 0) ? get_sector_pointer(0).get_first() : 0;
	for (int sp = 1; sp < sector_pointer_count(); sp++) {
		dvda_sector_pointer_t& sector_pointer = get_sector_pointer(sp);
		sector = (sector < sector_pointer.get_first()) ? sector : sector_pointer.get_first();
	}
	return sector; 
};

uint32_t dvda_track_t::get_last() {
	uint32_t sector = (sector_pointer_count() > 0) ? get_sector_pointer(0).get_last() : 0;
	for (int sp = 1; sp < sector_pointer_count(); sp++) {
		dvda_sector_pointer_t& sector_pointer = get_sector_pointer(sp);
		sector = (sector > sector_pointer.get_last()) ? sector : sector_pointer.get_last();
	}
	return sector; 
};


dvda_title_t::dvda_title_t(ats_title_t* p_ats_title, ats_title_idx_t* p_ats_title_idx) : dvda_object_t(DVDTypeTitle) {
	ats_title   = p_ats_title_idx->title_nr;
	ats_indexes = p_ats_title->indexes;
	ats_tracks  = p_ats_title->tracks;
	length_pts  = p_ats_title->len_in_pts;
}

dvda_title_t::~dvda_title_t() {
	tracks.clear();
}

int dvda_title_t::track_count() {
	return tracks.size();
}

dvda_track_t& dvda_title_t::get_track(int track_index) {
	return tracks[track_index];
}

void dvda_title_t::append(dvda_track_t& dvda_track) {
	tracks.push_back(dvda_track);
}

int dvda_title_t::get_title() {
	return ats_title;
}

double dvda_title_t::get_time() {
	return PTS_TO_SEC(length_pts);
}


dvda_downmix_channel_t* dvda_downmix_matrix_t::get_downmix_channel(int channel, int dmx_channel) {
	if (channel >= 0 && channel < DOWNMIX_CHANNELS && dmx_channel >= 0 && dmx_channel < 2) {
		return &LR_dmx[channel][dmx_channel];
	}
	return nullptr;
}

double dvda_downmix_matrix_t::get_downmix_coef(int channel, int dmx_channel) {
	double dmx_coef = 0.0;
	dvda_downmix_channel_t*	p_dmx_channel = get_downmix_channel(channel, dmx_channel);
	if (p_dmx_channel) {
		uint8_t coef = p_dmx_channel->coef;
		if (coef < 200) {
			double L_db = -0.2007 * coef;
			dmx_coef = pow(10.0, L_db / 20.0);
			if (p_dmx_channel->inv_phase)
				dmx_coef = -dmx_coef;
		}
		else if (coef < 255) {
			double L_db = -(2.0 * 0.2007 * (coef - 200) + 0.2007 * 200);
			dmx_coef = pow(10.0, L_db / 20.0);
			if (p_dmx_channel->inv_phase)
				dmx_coef = -dmx_coef;
		}
	}
	return dmx_coef;
}

dvda_titleset_t::dvda_titleset_t() : dvda_object_t(DVDTypeTitleset) {
	zone = nullptr;
}

dvda_titleset_t::~dvda_titleset_t() {
	if (zone) {
		close();
	}
	titles.clear();
}

int dvda_titleset_t::title_count() {
	return titles.size();
}

dvda_title_t& dvda_titleset_t::get_title(int title_index) {
	return titles[title_index];
}

void dvda_titleset_t::append(dvda_title_t& title) {
	titles.push_back(title);
}

uint32_t dvda_titleset_t::get_last() {
	return aobs_last_sector;
}

bool dvda_titleset_t::is_audio_ts() {
	return titleset_type == DVDTitlesetAudio;
}

bool dvda_titleset_t::is_video_ts() {
	return titleset_type == DVDTitlesetVideo;
}

double dvda_titleset_t::get_downmix_coef(int matrix, int channel, int dmx_channel) {
	if (matrix >= 0 && matrix < DOWNMIX_MATRICES) {
		return downmix_matrices[matrix].get_downmix_coef(channel, dmx_channel);
	}
	return 0.0;
}

bool dvda_titleset_t::open(dvda_zone_t* _zone, int titleset_index) {
	atsi_mat_t atsi_mat;
	titleset_type = DVDTitlesetUnknown;
	char file_name[13];
	snprintf(file_name, sizeof(file_name), "ATS_%02d_0.IFO", titleset_index + 1);
	dvda_fileobject_t* atsi_file = _zone->get_filesystem()->file_open(file_name);
	if (!atsi_file) {
		return false;
	}
	int64_t atsi_size = atsi_file->get_size();
	if (atsi_size >= 0x0800) {
		if (atsi_file->read((char*)&atsi_mat, sizeof(atsi_mat_t)) == sizeof(atsi_mat_t)) {
			if (memcmp("DVDAUDIO-ATS", atsi_mat.ats_identifier, 12) == 0) {
				uint32_t aob_offset = 0;
				for (int i = 0; i < 9; i++) {
					snprintf(file_name, sizeof(file_name), "ATS_%02d_%01d.AOB", titleset_index + 1, i + 1);
					aobs[i].dvda_fileobject = _zone->get_filesystem()->file_open(file_name);
					if (aobs[i].dvda_fileobject) {
						int64_t aob_size = aobs[i].dvda_fileobject->get_size();
						aobs[i].block_first = aob_offset;
						aobs[i].block_last = (uint32_t)(aobs[i].block_first + aob_size / DVD_BLOCK_SIZE + (aob_size % DVD_BLOCK_SIZE > 0 ? 1 : 0) - 1);
					}
					else {
						aobs[i].block_first = aob_offset;
						aobs[i].block_last = aobs[i].block_first + (1024 * 1024 - 32) * 1024 / DVD_BLOCK_SIZE - 1;
					}
					aob_offset = aobs[i].block_last + 1;
				}
				B2N_32(atsi_mat.ats_last_sector);
				B2N_32(atsi_mat.atsi_last_sector);
				B2N_32(atsi_mat.ats_category);
				B2N_32(atsi_mat.atsi_last_byte);
				B2N_32(atsi_mat.atsm_vobs);
				B2N_32(atsi_mat.atstt_vobs);
				B2N_32(atsi_mat.ats_ptt_srpt);
				B2N_32(atsi_mat.ats_pgcit);
				B2N_32(atsi_mat.atsm_pgci_ut);
				B2N_32(atsi_mat.ats_tmapt);
				B2N_32(atsi_mat.atsm_c_adt);
				B2N_32(atsi_mat.atsm_vobu_admap);
				B2N_32(atsi_mat.ats_c_adt);
				B2N_32(atsi_mat.ats_vobu_admap);
				for (int i = 0; i < 8; i++) {
					B2N_16(atsi_mat.ats_audio_format[i].audio_type);
				}
				for (int m = 0; m < DOWNMIX_MATRICES; m++) {
					for (int ch = 0; ch < DOWNMIX_CHANNELS; ch++) {
						downmix_matrices[m].get_downmix_channel(ch, 0)->inv_phase = ((atsi_mat.ats_downmix_matrices[m].phase.L >> (DOWNMIX_CHANNELS - ch - 1)) & 1) == 1;
						downmix_matrices[m].get_downmix_channel(ch, 0)->coef = atsi_mat.ats_downmix_matrices[m].coef[ch].L;
						downmix_matrices[m].get_downmix_channel(ch, 1)->inv_phase = ((atsi_mat.ats_downmix_matrices[m].phase.R >> (DOWNMIX_CHANNELS - ch - 1)) & 1) == 1;
						downmix_matrices[m].get_downmix_channel(ch, 1)->coef = atsi_mat.ats_downmix_matrices[m].coef[ch].R;
					}
				}
				if (atsi_mat.atsm_vobs == 0) {
					titleset_type = DVDTitlesetAudio;
				}
				else {
					titleset_type = DVDTitlesetVideo;
				}
				aobs_last_sector = atsi_mat.ats_last_sector - 2 * (atsi_mat.atsi_last_sector + 1);
				uint32_t ats_len = (uint32_t)atsi_size - 0x0800;
				atsi_file->seek(0x0800);
				uint8_t* ats_buf = new uint8_t[ats_len];
				uint8_t* ats_end = ats_buf + ats_len;
				atsi_file->read((char*)ats_buf, ats_len);
				audio_pgcit_t* p_audio_pgcit = (audio_pgcit_t*)ats_buf;
				if ((uint8_t*)p_audio_pgcit + AUDIO_PGCIT_SIZE <= ats_end) {
					B2N_16(p_audio_pgcit->nr_of_titles);
					B2N_32(p_audio_pgcit->last_byte);
					ats_end = ats_buf + ((ats_len < p_audio_pgcit->last_byte + 1) ? ats_len : p_audio_pgcit->last_byte + 1);
					ats_title_idx_t* p_ats_title_idx = (ats_title_idx_t*)((uint8_t*)p_audio_pgcit + AUDIO_PGCIT_SIZE);
					for (int i = 0; i < p_audio_pgcit->nr_of_titles; i++) {
						if ((uint8_t*)&p_ats_title_idx[i] + ATS_TITLE_IDX_SIZE > ats_end) {
							break;
						}
						B2N_32(p_ats_title_idx[i].title_table_offset);
						ats_title_t* p_ats_title = (ats_title_t*)((uint8_t*)p_audio_pgcit + p_ats_title_idx[i].title_table_offset);
						if ((uint8_t*)p_ats_title + ATS_TITLE_SIZE > ats_end) {
							break;
						}
						B2N_32(p_ats_title->len_in_pts);
						B2N_16(p_ats_title->track_sector_table_offset);
						ats_track_timestamp_t* p_ats_track_timestamp = (ats_track_timestamp_t*)((uint8_t*)p_ats_title + ATS_TITLE_SIZE);
						ats_track_sector_t* p_ats_track_sector = (ats_track_sector_t*)((uint8_t*)p_ats_title + p_ats_title->track_sector_table_offset);
						dvda_title_t t(p_ats_title, &p_ats_title_idx[i]);
						append(t);
						dvda_title_t& title = get_title(i);
						for (int j = 0; j < p_ats_title->tracks; j++) {
							if ((uint8_t*)&p_ats_track_timestamp[j] + ATS_TRACK_TIMESTAMP_SIZE > ats_end) {
								break;
							}
							B2N_32(p_ats_track_timestamp[j].first_pts);
							B2N_32(p_ats_track_timestamp[j].len_in_pts);
							dvda_track_t track(&p_ats_track_timestamp[j], j + 1);
							title.append(track);
						}
						for (int j = 0; j < p_ats_title->indexes; j++) {
							if ((uint8_t*)&p_ats_track_sector[j] + ATS_TRACK_SECTOR_SIZE > ats_end) {
								break;
							}
							B2N_32(p_ats_track_sector[j].first);
							B2N_32(p_ats_track_sector[j].last);
							for (int k = 0; k < title.track_count(); k++) {
								int track_curr_idx, track_next_idx;
								dvda_track_t& track = get_title(i).get_track(k);
								track_curr_idx = track.get_index();
								track_next_idx = (k < title.track_count() - 1) ? title.get_track(k + 1).get_index() : 0;
								if (j + 1 >= track_curr_idx && (j + 1 < track_next_idx || track_next_idx == 0)) {
									dvda_sector_pointer_t sector_pointer(&track, &p_ats_track_sector[j], j + 1);
									track.append(sector_pointer);
								}
							}
						}
						/*
						for (int j = 0; j < title.track_count(); j++) {
							dvda_track_t& track = title.get_track(j);
						}
						*/
					}
					zone = _zone;
				}
				delete ats_buf;
			}
		}
		_zone->get_filesystem()->file_close(atsi_file);
	}
	return zone != nullptr;
}

void dvda_titleset_t::close() {
	if (zone) {
		for (int i = 0; i < 9; i++) {
			if (aobs[i].dvda_fileobject) {
				aobs[i].dvda_fileobject->close();
			}
		}
		titles.clear();
		zone = nullptr;
	}
}

DVDAERROR dvda_titleset_t::get_block(uint32_t block_index, uint8_t* block_data) {
	for (int i = 0; i < 9; i++) {
		if (aobs[i].dvda_fileobject && block_index >= aobs[i].block_first && block_index <= aobs[i].block_last) {
			if (!aobs[i].dvda_fileobject->seek((block_index - aobs[i].block_first) * DVD_BLOCK_SIZE))
				return DVDAERR_CANNOT_SEEK_ATS_XX_X_AOB;
			if (aobs[i].dvda_fileobject->read((char*)block_data, DVD_BLOCK_SIZE) != DVD_BLOCK_SIZE)
				return DVDAERR_CANNOT_READ_ATS_XX_X_AOB;
			return DVDAERR_OK;
		}
	}
	return DVDAERR_AOB_BLOCK_NOT_FOUND;
}

int dvda_titleset_t::get_blocks(uint32_t block_first, uint32_t block_last, uint8_t* block_data) {
	int blocks_read = 0;
	int aob_index = -1;
	for (int i = 0; i < 9; i++) {
		if (block_first >= aobs[i].block_first && block_first <= aobs[i].block_last) {
			aob_index = i;
			break;
		}
	}
	if (aob_index >= 0) {
		if (aobs[aob_index].dvda_fileobject) {
			if (aobs[aob_index].dvda_fileobject->seek((block_first - aobs[aob_index].block_first) * DVD_BLOCK_SIZE)) {
				if (block_last <= aobs[aob_index].block_last) {
					int bytes_to_read = (block_last + 1 - block_first) * DVD_BLOCK_SIZE;
					int bytes_read = aobs[aob_index].dvda_fileobject->read((char*)block_data, bytes_to_read);
					blocks_read += bytes_read / DVD_BLOCK_SIZE;
				}
				else {
					int bytes_to_read_1 = (aobs[aob_index].block_last + 1 - block_first) * DVD_BLOCK_SIZE;
					int bytes_read_1 = aobs[aob_index].dvda_fileobject->read((char*)block_data, bytes_to_read_1);
					blocks_read += bytes_read_1 / DVD_BLOCK_SIZE;
					if (aob_index + 1 < 9) {
						if (aobs[aob_index + 1].dvda_fileobject) {
							if (aobs[aob_index + 1].dvda_fileobject->seek(0)) {
								int bytes_to_read_2 = (block_last + 1 - aobs[aob_index + 1].block_first) * DVD_BLOCK_SIZE;
								int bytes_read_2 = aobs[aob_index + 1].dvda_fileobject->read((char*)block_data + blocks_read * DVD_BLOCK_SIZE, bytes_to_read_2);
								blocks_read += bytes_read_2 / DVD_BLOCK_SIZE;
							}
						}
					}
				}
			}
		}
	}
	return blocks_read;
}


dvda_zone_t::dvda_zone_t() : dvda_object_t(DVDTypeZone) {
	filesystem = nullptr;
}

dvda_zone_t::~dvda_zone_t() {
}

dvda_filesystem_t* dvda_zone_t::get_filesystem() {
	return filesystem;
}

int dvda_zone_t::titleset_count() {
	int size;
	size = titlesets.size();
	return size;
}

dvda_titleset_t& dvda_zone_t::get_titleset(int titleset_index) {
	return titlesets[titleset_index];
}

void dvda_zone_t::append(dvda_titleset_t& titleset) {
	titlesets.push_back(titleset);
}

bool dvda_zone_t::open(dvda_filesystem_t* _filesystem) {
	if (filesystem) {
		close();
	}
	amgi_mat_t amgi_mat;
	audio_titlesets = 99;
	video_titlesets = 99;
	dvda_fileobject_t* amgi_file = _filesystem->file_open("AUDIO_TS.IFO");
	if (amgi_file) {
		if (amgi_file->read( &amgi_mat, sizeof(amgi_mat_t)) == sizeof(amgi_mat_t)) {
			if (memcmp("DVDAUDIO-AMG", amgi_mat.amg_identifier, 12) == 0) {
				B2N_32(amgi_mat.amg_last_sector);
				B2N_32(amgi_mat.amgi_last_sector);
				B2N_32(amgi_mat.amg_category);
				B2N_16(amgi_mat.amg_nr_of_volumes);
				B2N_16(amgi_mat.amg_this_volume_nr);
				B2N_32(amgi_mat.amg_asvs);
				B2N_64(amgi_mat.amg_pos_code);
				B2N_32(amgi_mat.amgi_last_byte);
				B2N_32(amgi_mat.first_play_pgc);
				B2N_32(amgi_mat.amgm_vobs);
				B2N_32(amgi_mat.att_srpt);
				B2N_32(amgi_mat.aott_srpt);
				B2N_32(amgi_mat.amgm_pgci_ut);
				B2N_32(amgi_mat.ats_atrt);
				B2N_32(amgi_mat.txtdt_mgi);
				B2N_32(amgi_mat.amgm_c_adt);
				B2N_32(amgi_mat.amgm_vobu_admap);
				B2N_16(amgi_mat.amgm_audio_attr.lang_code);
				B2N_16(amgi_mat.amgm_subp_attr.lang_code);
				audio_titlesets = (audio_titlesets < amgi_mat.amg_nr_of_audio_title_sets) ? audio_titlesets : amgi_mat.amg_nr_of_audio_title_sets;
				video_titlesets = (video_titlesets < amgi_mat.amg_nr_of_video_title_sets) ? video_titlesets : amgi_mat.amg_nr_of_video_title_sets;
				filesystem = _filesystem;
				for (int ts = 0; ts < audio_titlesets; ts++) {
					dvda_titleset_t titleset;
					if (titleset.open(this, ts)) {
						append(titleset);
					}
				}
				/*
				for (int ts = 0; ts < titleset_count(); ts++) {
					dvda_titleset_t& titleset = get_titleset(ts);
					for (int ti = 0; ti < titleset.title_count(); ti++) {
						dvda_title_t& title = titleset.get_title(ti);
						for (int tr = 0; tr < title.track_count(); tr++) {
							dvda_track_t& track = title.get_track(tr);
							for (int sp = 0; sp < track.sector_pointer_count(); sp++) {
								dvda_sector_pointer_t& sector_pointer = track.get_sector_pointer(sp);
							}
						}
					}
				}
				*/
			}
		}
		_filesystem->file_close(amgi_file);
	}
	return filesystem != nullptr;
}

void dvda_zone_t::close() {
	if (filesystem) {
		titlesets.clear();
		filesystem = nullptr;
	}
}

DVDAERROR dvda_zone_t::get_block(int titleset_index, uint32_t block_index, uint8_t* block_data) {
	return get_titleset(titleset_index).get_block(block_index, block_data);
}

int dvda_zone_t::get_blocks(int titleset_index, uint32_t block_index, int block_count, uint8_t* block_data) {
	return get_titleset(titleset_index).get_blocks(block_index, block_index + block_count - 1, block_data);
}
