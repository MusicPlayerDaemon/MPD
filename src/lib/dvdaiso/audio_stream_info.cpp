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

#include "audio_stream_info.h"

const MLPPCM_ASSIGNMENT audio_stream_info_t::mlppcm_table[21] = {
	/*  0 */ {{SPEAKER_FRONT_CENTER},                                                        {},                                                                                {"M"},                 {},                    1, 0},
	/*  1 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT},                                      {},                                                                                {"L","R"},             {},                    2, 0},
	/*  2 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT},                                      {SPEAKER_BACK_CENTER},                                                             {"Lf","Rf"},           {"S"},                 2, 1},
	/*  3 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT},                                      {SPEAKER_BACK_LEFT,SPEAKER_BACK_RIGHT},                                            {"Lf","Rf"},           {"Ls","Rs"},           2, 2},
	/*  4 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT},                                      {SPEAKER_LOW_FREQUENCY},                                                           {"Lf","Rf"},           {"LFE"},               2, 1},
	/*  5 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT},                                      {SPEAKER_LOW_FREQUENCY,SPEAKER_BACK_CENTER},                                       {"Lf","Rf"},           {"LFE","S"},           2, 2},
	/*  6 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT},                                      {SPEAKER_LOW_FREQUENCY,SPEAKER_BACK_LEFT,SPEAKER_BACK_RIGHT},                      {"Lf","Rf"},           {"LFE","Ls","Rs"},     2, 3},
	/*  7 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT},                                      {SPEAKER_FRONT_CENTER},                                                            {"Lf","Rf"},           {"C"},                 2, 1},
	/*  8 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT},                                      {SPEAKER_FRONT_CENTER,SPEAKER_BACK_CENTER},                                        {"Lf","Rf"},           {"C","S"},             2, 2},
	/*  9 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT},                                      {SPEAKER_FRONT_CENTER,SPEAKER_BACK_LEFT,SPEAKER_BACK_RIGHT},                       {"Lf","Rf"},           {"C","Ls","Rs"},       2, 3},
	/* 10 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT},                                      {SPEAKER_FRONT_CENTER,SPEAKER_LOW_FREQUENCY},                                      {"Lf","Rf"},           {"C","LFE"},           2, 2},
	/* 11 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT},                                      {SPEAKER_FRONT_CENTER,SPEAKER_LOW_FREQUENCY,SPEAKER_BACK_CENTER},                  {"Lf","Rf"},           {"C","LFE","S"},       2, 3},
	/* 12 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT},                                      {SPEAKER_FRONT_CENTER,SPEAKER_LOW_FREQUENCY,SPEAKER_BACK_LEFT,SPEAKER_BACK_RIGHT}, {"Lf","Rf"},           {"C","LFE","Ls","Rs"}, 2, 4},
	/* 13 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT,SPEAKER_FRONT_CENTER},                 {SPEAKER_BACK_CENTER},                                                             {"Lf","Rf","C"},       {"S"},                 3, 1},
	/* 14 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT,SPEAKER_FRONT_CENTER},                 {SPEAKER_BACK_LEFT,SPEAKER_BACK_RIGHT},                                            {"Lf","Rf","C"},       {"Ls","Rs"},           3, 2},
	/* 15 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT,SPEAKER_FRONT_CENTER},                 {SPEAKER_LOW_FREQUENCY},                                                           {"Lf","Rf","C"},       {"LFE"},               3, 1},
	/* 16 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT,SPEAKER_FRONT_CENTER},                 {SPEAKER_LOW_FREQUENCY,SPEAKER_BACK_CENTER},                                       {"Lf","Rf","C"},       {"LFE","S"},           3, 2},
	/* 17 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT,SPEAKER_FRONT_CENTER},                 {SPEAKER_LOW_FREQUENCY,SPEAKER_BACK_LEFT,SPEAKER_BACK_RIGHT},                      {"Lf","Rf","C"},       {"LFE","Ls","Rs"},     3, 3},
	/* 18 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT,SPEAKER_BACK_LEFT,SPEAKER_BACK_RIGHT}, {SPEAKER_LOW_FREQUENCY},                                                           {"Lf","Rf","Ls","Rs"}, {"LFE"},               4, 1},
	/* 19 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT,SPEAKER_BACK_LEFT,SPEAKER_BACK_RIGHT}, {SPEAKER_FRONT_CENTER},                                                            {"Lf","Rf","Ls","Rs"}, {"C"},                 4, 1},
	/* 20 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT,SPEAKER_BACK_LEFT,SPEAKER_BACK_RIGHT}, {SPEAKER_FRONT_CENTER,SPEAKER_LOW_FREQUENCY},                                      {"Lf","Rf","Ls","Rs"}, {"C","LFE"},           4, 2},
};

//  TrueHD channel map
//  LR    C   LFE  LRs LRvh  LRc LRrs  Cs   Ts  LRsd  LRw  Cvh  LFE2
const TRUEHD_ASSIGNMENT audio_stream_info_t::truehd_table[13] = {
	/*  0 */ {{SPEAKER_FRONT_LEFT,SPEAKER_FRONT_RIGHT},                     {"L","R"},     2},
	/*  1 */ {{SPEAKER_FRONT_CENTER},                                       {"C"},         1},
	/*  2 */ {{SPEAKER_LOW_FREQUENCY},                                      {"LFE"},       1},
	/*  3 */ {{SPEAKER_BACK_LEFT,SPEAKER_BACK_RIGHT},                       {"Ls","Rs"},   2},
	/*  4 */ {{SPEAKER_TOP_FRONT_LEFT,SPEAKER_TOP_FRONT_RIGHT},             {"Lvh","Rvh"}, 2},
	/*  5 */ {{SPEAKER_FRONT_LEFT_OF_CENTER,SPEAKER_FRONT_RIGHT_OF_CENTER}, {"Lc","Rc"},   2},
	/*  6 */ {{SPEAKER_TOP_BACK_LEFT,SPEAKER_TOP_BACK_RIGHT},               {"Lrs","Rrs"}, 2},
	/*  7 */ {{SPEAKER_BACK_CENTER},                                        {"Cs"},        1},
	/*  8 */ {{SPEAKER_TOP_BACK_CENTER},                                    {"Ts"},        1},
	/*  9 */ {{SPEAKER_SIDE_LEFT,SPEAKER_SIDE_RIGHT},                       {"Lsd","Rsd"}, 2},
	/* 10 */ {{SPEAKER_SIDE_LEFT,SPEAKER_SIDE_RIGHT},                       {"Lw","Rw"},   2},
	/* 11 */ {{SPEAKER_TOP_CENTER},                                         {"Cvh"},       1},
	/* 12 */ {{SPEAKER_LOW_FREQUENCY},                                      {"LFE2"},      1}
};

audio_stream_info_t::audio_stream_info_t() {
	stream_id = 0;
}

const char* audio_stream_info_t::get_channel_name(int channel) {
	if (((stream_id == MLP_STREAM_ID && stream_type == STREAM_TYPE_MLP) || stream_id == PCM_STREAM_ID) && channel_assignment >= 0 && channel_assignment <= 20) {
		if (channel >= 0 && channel < mlppcm_table[channel_assignment].group1_channels)
			return mlppcm_table[channel_assignment].group1_channel_name[channel];
		if (channel >= mlppcm_table[channel_assignment].group1_channels && channel < mlppcm_table[channel_assignment].group1_channels + mlppcm_table[channel_assignment].group2_channels)
			return mlppcm_table[channel_assignment].group2_channel_name[channel - mlppcm_table[channel_assignment].group1_channels];
	}
	else if (stream_id == MLP_STREAM_ID && stream_type == STREAM_TYPE_TRUEHD) {
		int ch = 0;
		for (int i = 0; i < 13; i++) {
			if ((channel_assignment >> i) & 1) {
				for (int j = 0; j < truehd_table[i].channels; j++) {
					if (ch++ == channel)
						return truehd_table[i].channel_name[j];
				}
			}
		}
	}
	return "";
}

uint32_t audio_stream_info_t::get_wfx_channels() {
	uint32_t wfx_channels = 0;
	if (((stream_id == MLP_STREAM_ID && stream_type == STREAM_TYPE_MLP) || stream_id == PCM_STREAM_ID) && channel_assignment >= 0 && channel_assignment <= 20) {
		for (int i = 0; i < mlppcm_table[channel_assignment].group1_channels; i++)
			wfx_channels |= mlppcm_table[channel_assignment].group1_channel_id[i];
		for (int i = 0; i < mlppcm_table[channel_assignment].group2_channels; i++)
			wfx_channels |= mlppcm_table[channel_assignment].group2_channel_id[i];
	}
	else if (stream_id == MLP_STREAM_ID && stream_type == STREAM_TYPE_TRUEHD) {
		for (int i = 0; i < 13; i++) {
			if ((channel_assignment >> i) & 1)
				for (int j = 0; j < truehd_table[i].channels; j++)
					wfx_channels |= truehd_table[i].channel_id[j];
		}
	}
	return wfx_channels;
}

double audio_stream_info_t::estimate_compression() {
	double compression = 1.0;
	if (stream_id == MLP_STREAM_ID) {
		switch (group1_samplerate) {
		case 44100:
		case 48000:
			compression = (double)group1_bits / ((double)group1_bits - 4.0);
			break;
		case 88200:
		case 96000:
			compression = (double)group1_bits / ((double)group1_bits - 8.0);
			break;
		case 176400:
		case 192000:
			compression = (double)group1_bits / ((double)group1_bits - 9.0);
			break;
		}
	}
	return compression;
}



