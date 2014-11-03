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

dvda_sector_pointer_t::dvda_sector_pointer_t(dvda_track_t* _dvda_track, ats_track_sector_t* p_ats_track_sector, int _index) {
	obj_type = DVDATypeSectorPointer;
	dvda_track = _dvda_track;
	index = _index;
	first = p_ats_track_sector->first;
	last  = p_ats_track_sector->last;
}

double dvda_sector_pointer_t::get_time() {
	return PTS_TO_SEC(get_length_pts());
}

uint32_t dvda_sector_pointer_t::get_length_pts() {
	uint32_t denom = dvda_track->get_last() - dvda_track->get_first() + 1;
	if (denom) {
		double pts = (double)dvda_track->get_length_pts() * (double)(last - first + 1) / (double)denom;
		return (uint32_t)pts;
	}
	return 0;
}

dvda_track_t::dvda_track_t(ats_track_timestamp_t* p_ats_track_timestamp, int track) {
	obj_type       = DVDATypeTrack;
	this->track    = track;
	index          = p_ats_track_timestamp->n;
	first_pts      = p_ats_track_timestamp->first_pts;
	length_pts     = p_ats_track_timestamp->len_in_pts;
	downmix_matrix = p_ats_track_timestamp->downmix_matrix < DOWNMIX_MATRICES ? p_ats_track_timestamp->downmix_matrix : -1;
}

dvda_track_t::~dvda_track_t() {
	for (int i = 0; i < get_sector_pointers(); i++)
		delete get_sector_pointer(i);
	dvda_sector_pointers.clear();
}

uint32_t dvda_track_t::get_first() {
	uint32_t sector = (get_sector_pointers() > 0) ? get_sector_pointer(0)->get_first() : 0;
	for (int i = 1; i < get_sector_pointers(); i++) {
		dvda_sector_pointer_t* dvda_sector_pointer = get_sector_pointer(i);
		if (dvda_sector_pointer)
			sector = (sector < dvda_sector_pointer->get_first()) ? sector : dvda_sector_pointer->get_first();
	}
	return sector; 
};

uint32_t dvda_track_t::get_last() {
	uint32_t sector = (get_sector_pointers() > 0) ? get_sector_pointer(0)->get_last() : 0;
	for (int i = 1; i < get_sector_pointers(); i++) {
		dvda_sector_pointer_t* dvda_sector_pointer = get_sector_pointer(i);
		if (dvda_sector_pointer)
			sector = (sector > dvda_sector_pointer->get_last()) ? sector : dvda_sector_pointer->get_last();
	}
	return sector; 
};

dvda_title_t::dvda_title_t(ats_title_t* p_ats_title, ats_title_idx_t* p_ats_title_idx) {
	obj_type = DVDATypeTitle;
	title    = p_ats_title_idx->title_nr;
	indexes  = p_ats_title->indexes;
	tracks   = p_ats_title->tracks;
	length_pts = p_ats_title->len_in_pts;
}

dvda_title_t::~dvda_title_t() {
	for (int i = 0; i < get_tracks(); i++) {
		delete get_track(i);
	}
	dvda_tracks.clear();
}

dvda_downmix_channel_t* dvda_downmix_matrix_t::get_downmix_channel(int channel, int dmx_channel) {
	if (channel >= 0 && channel < DOWNMIX_CHANNELS && dmx_channel >= 0 && dmx_channel < 2)
		return &LR_dmx[channel][dmx_channel];
	return NULL;
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

bool dvda_titleset_t::open(dvda_zone_t* dvda_zone, int i_titleset) {
	obj_type = DVDATypeTitleset;
	this->dvda_zone = dvda_zone;
	atsi_mat_t atsi_mat;
	titleset = i_titleset + 1;
	dvda_titleset_type = DVDTitlesetUnknown;
	char file_name[13];
	snprintf(file_name, sizeof(file_name), "ATS_%02d_0.IFO", titleset);
	dvda_fileobject_t* atsi_file = dvda_zone->get_filesystem()->file_open(file_name);
	if (!atsi_file)
		return is_open;
	int64_t atsi_size = atsi_file->get_size();
	if (atsi_size >= 0x0800) {
		if (atsi_file->read((char*)&atsi_mat, sizeof(atsi_mat_t)) == sizeof(atsi_mat_t)) {
			if (memcmp("DVDAUDIO-ATS", atsi_mat.ats_identifier, 12) == 0) {
				uint32_t aob_offset = 0;
				for (int i = 0; i < 9; i++) {
					snprintf(file_name, sizeof(file_name), "ATS_%02d_%01d.AOB", titleset, i + 1);
					aobs[i].dvda_fileobject = dvda_zone->get_filesystem()->file_open(file_name);
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
				if (atsi_mat.atsm_vobs == 0)
					dvda_titleset_type = DVDTitlesetAudio;
				else
					dvda_titleset_type = DVDTitlesetVideo;
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
						if ((uint8_t*)&p_ats_title_idx[i] + ATS_TITLE_IDX_SIZE > ats_end)
							break;
						B2N_32(p_ats_title_idx[i].title_table_offset);
						ats_title_t* p_ats_title = (ats_title_t*)((uint8_t*)p_audio_pgcit + p_ats_title_idx[i].title_table_offset);
						if ((uint8_t*)p_ats_title + ATS_TITLE_SIZE > ats_end)
							break;
						B2N_32(p_ats_title->len_in_pts);
						B2N_16(p_ats_title->track_sector_table_offset);
						ats_track_timestamp_t* p_ats_track_timestamp = (ats_track_timestamp_t*)((uint8_t*)p_ats_title + ATS_TITLE_SIZE);
						ats_track_sector_t* p_ats_track_sector = (ats_track_sector_t*)((uint8_t*)p_ats_title + p_ats_title->track_sector_table_offset);
						dvda_title_t* dvda_title = new dvda_title_t(p_ats_title, &p_ats_title_idx[i]);
						add_title(dvda_title);
						for (int j = 0; j < p_ats_title->tracks; j++) {
							if ((uint8_t*)&p_ats_track_timestamp[j] + ATS_TRACK_TIMESTAMP_SIZE > ats_end)
								break;
							B2N_32(p_ats_track_timestamp[j].first_pts);
							B2N_32(p_ats_track_timestamp[j].len_in_pts);
							dvda_track_t* dvda_track = new dvda_track_t(&p_ats_track_timestamp[j], j + 1);
							dvda_title->add_track(dvda_track);
						}
						for (int j = 0; j < p_ats_title->indexes; j++) {
							if ((uint8_t*)&p_ats_track_sector[j] + ATS_TRACK_SECTOR_SIZE > ats_end)
								break;
							B2N_32(p_ats_track_sector[j].first);
							B2N_32(p_ats_track_sector[j].last);
							for (int k = 0; k < dvda_title->get_tracks(); k++) {
								int track_curr_idx, track_next_idx;
								dvda_track_t* dvda_track = dvda_title->get_track(k);
								track_curr_idx = dvda_track->get_index();
								track_next_idx = (k < dvda_title->get_tracks() - 1) ? dvda_title->get_track(k + 1)->get_index() : 0;
								if (j + 1 >= track_curr_idx && (j + 1 < track_next_idx || track_next_idx == 0)) {
									dvda_sector_pointer_t* dvda_sector_pointer = new dvda_sector_pointer_t(dvda_track, &p_ats_track_sector[j], j + 1);
									dvda_track->add_sector_pointer(dvda_sector_pointer);
								}
							}
						}
						for (int j = 0; j < dvda_title->get_tracks(); j++) {
							dvda_track_t* dvda_track = dvda_title->get_track(j);
						}
					}
					is_open = true;
				}
				delete ats_buf;
			}
		}
		dvda_zone->get_filesystem()->file_close(atsi_file);
	}
	return is_open;
}

dvda_titleset_t::~dvda_titleset_t() {
	if (is_open) {
		close_aobs();
	}
	for (int i = 0; i < get_titles(); i++) {
		delete get_title(i);
	}
	dvda_titles.clear();
}

DVDAERROR dvda_titleset_t::get_block(uint32_t block, uint8_t* buf_ptr) {
	for (int i = 0; i < 9; i++) {
		if (aobs[i].dvda_fileobject && block >= aobs[i].block_first && block <= aobs[i].block_last) {
			if (!aobs[i].dvda_fileobject->seek((block - aobs[i].block_first) * DVD_BLOCK_SIZE))
				return DVDAERR_CANNOT_SEEK_ATS_XX_X_AOB;
			if (aobs[i].dvda_fileobject->read((char*)buf_ptr, DVD_BLOCK_SIZE) != DVD_BLOCK_SIZE)
				return DVDAERR_CANNOT_READ_ATS_XX_X_AOB;
			return DVDAERR_OK;
		}
	}
	return DVDAERR_AOB_BLOCK_NOT_FOUND;
}

int dvda_titleset_t::get_blocks(uint32_t block_first, uint32_t block_last, uint8_t* buf_ptr) {
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
					int bytes_read = aobs[aob_index].dvda_fileobject->read((char*)buf_ptr, bytes_to_read);
					blocks_read += bytes_read / DVD_BLOCK_SIZE;
				}
				else {
					int bytes_to_read_1 = (aobs[aob_index].block_last + 1 - block_first) * DVD_BLOCK_SIZE;
					int bytes_read = aobs[aob_index].dvda_fileobject->read((char*)buf_ptr, bytes_to_read_1);
					blocks_read += bytes_read / DVD_BLOCK_SIZE;
					if (aob_index + 1 < 9) {
						if (aobs[aob_index + 1].dvda_fileobject) {
							if (aobs[aob_index + 1].dvda_fileobject->seek(0)) {
								int bytes_to_read_2 = (block_last + 1 - aobs[aob_index + 1].block_first) * DVD_BLOCK_SIZE;
								int bytes_read = aobs[aob_index + 1].dvda_fileobject->read((char*)buf_ptr + blocks_read * DVD_BLOCK_SIZE, bytes_to_read_2);
								blocks_read += bytes_read / DVD_BLOCK_SIZE;
							}
						}
					}
				}
			}
		}
	}
	return blocks_read;
}

void dvda_titleset_t::close_aobs() {
	for (int i = 0; i < 9; i++) {
		if (aobs[i].dvda_fileobject) {
			aobs[i].dvda_fileobject->close();
		}
	}
}

bool dvda_zone_t::open(dvda_filesystem_t* _dvda_filesystem) {
	if (is_open) {
		close();
		is_open = false;
	}
	obj_type = DVDATypeZone;
	dvda_filesystem = _dvda_filesystem;
	amgi_mat_t amgi_mat;
	audio_titlesets = 99;
	video_titlesets = 99;
	dvda_fileobject_t* amgi_file = dvda_filesystem->file_open("AUDIO_TS.IFO");
	if (amgi_file) {
		if (amgi_file->read((char*)&amgi_mat, sizeof(amgi_mat_t)) == sizeof(amgi_mat_t)) {
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
				for (int i_titleset = 0; i_titleset < audio_titlesets; i_titleset++) {
					dvda_titleset_t* dvda_titleset = new dvda_titleset_t();
					if (dvda_titleset->open(this, i_titleset))
						add_titleset(dvda_titleset);
					else
						delete dvda_titleset;
				}
				for (int i_titleset = 0; i_titleset < get_titlesets(); i_titleset++) {
					dvda_titleset_t* dvda_titleset = get_titleset(i_titleset);
					for (int i_title = 0; i_title < dvda_titleset->get_titles(); i_title++) {
						dvda_title_t* dvda_title = dvda_titleset->get_title(i_title);
						for (int i_track = 0; i_track < dvda_title->get_tracks(); i_track++) {
							dvda_track_t* dvda_track = dvda_title->get_track(i_track);
							for (int i_index = 0; i_index < dvda_track->get_sector_pointers(); i_index++) {
								dvda_sector_pointer_t* dvda_sector_pointer = dvda_track->get_sector_pointer(i_index);
							}
						}
					}
				}
				is_open = true;
			}
		}
		dvda_filesystem->file_close(amgi_file);
	}
	return is_open;
}

void dvda_zone_t::close() {
	if (is_open) {
		for (int i = 0; i < get_titlesets(); i++) {
			delete get_titleset(i);
		}
		dvda_titlesets.clear();
		is_open = false;
	}
}

DVDAERROR dvda_zone_t::get_block(int titleset, uint32_t block_no, uint8_t* buf_ptr) {
	DVDAERROR err;
	err = get_titleset(titleset)->get_block(block_no, buf_ptr);
	return err;
}

int dvda_zone_t::get_blocks(int titleset, uint32_t block_no, int blocks, uint8_t* buf_ptr) {
	blocks = get_titleset(titleset)->get_blocks(block_no, block_no + blocks - 1, buf_ptr);
	return blocks;
}
