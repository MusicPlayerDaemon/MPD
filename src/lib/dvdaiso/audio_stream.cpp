/*
* MPD DVD-Audio Decoder plugin
* Copyright (c) 2014 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
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

#include "audio_stream.h"

extern "C" AVCodecParser mlp_parser;
extern "C" AVCodec       mlp_decoder;

#ifndef min
	#define min(a, b) ((int)(a) < (int)(b) ? (a) : (b))
#endif

#define NULL_DECODE(data, data_size, buf, buf_size) \
	*data_size = *data_size < 1920 ? *data_size : 1920; \
	memset(data, 0, *data_size); \
	update_stats(500, *data_size); \
	return 500;

int32_t audio_stream_t::conv_sample(double sample) {
	double lim = (double)(((uint32_t)1 << ((group1_bits > 16 ? 32 : 16) - 1)) - 1);
	sample += 0.5;
	if (sample > lim)
		sample = lim;
	else if (sample < -lim)
		sample = -lim;
	return (int32_t)sample;
}

void audio_stream_t::reorder_channels(uint8_t* data, int* data_size) {
	if (stream_id == MLP_STREAM_ID && stream_type == STREAM_TYPE_TRUEHD)
		return;
	if (channel_assignment == 33) {
		switch (group1_bits) {
		case 16:
			for (int offset = 0; offset < *data_size; offset += (group1_channels + group2_channels) * sizeof(int16_t)) { 
				int16_t* sample = (int16_t*)(data + offset);
				sample[0] = 0;
			}
			break;
		case 20:
		case 24:
			for (int offset = 0; offset < *data_size; offset += (group1_channels + group2_channels) * sizeof(int32_t)) { 
				int32_t* sample = (int32_t*)(data + offset);
				sample[0] = 0;
				sample[1] = 0;
				sample[2] = 0;
				//sample[3] = 0;
			}
			break;
		default:
			break;
		}
		return;
	}
	if (channel_assignment < 18)
		return;
	switch (group1_bits) {
	case 16:
		for (int offset = 0; offset < *data_size; offset += (group1_channels + group2_channels) * sizeof(int16_t)) { 
			int16_t* sample = (int16_t*)(data + offset);
			int16_t Ls = sample[2];
			int16_t Rs = sample[3];
			for (int i = 0; i < group2_channels; i++)
				sample[2 + i] = sample[group1_channels + i];
			sample[2 + group2_channels + 0] = Ls;
			sample[2 + group2_channels + 1] = Rs;
		}
		break;
	case 20:
	case 24:
		for (int offset = 0; offset < *data_size; offset += (group1_channels + group2_channels) * sizeof(int32_t)) { 
			int32_t* sample = (int32_t*)(data + offset);
			int32_t Ls = sample[2];
			int32_t Rs = sample[3];
			for (int i = 0; i < group2_channels; i++)
				sample[2 + i] = sample[group1_channels + i];
			sample[2 + group2_channels + 0] = Ls;
			sample[2 + group2_channels + 1] = Rs;
		}
		break;
	default:
		break;
	}
}

void audio_stream_t::set_downmix_coef() {
	// Ldmx
	LR_dmx_coef[0][0] = +0.500; // Lf
	LR_dmx_coef[1][0] = +0.000; // Rf
	LR_dmx_coef[2][0] = +0.354; // C
	LR_dmx_coef[3][0] = +0.177; // LFE
	LR_dmx_coef[4][0] = +0.250; // Ls
	LR_dmx_coef[5][0] = +0.000; // Rs
	LR_dmx_coef[6][0] = +0.000;
	LR_dmx_coef[7][0] = +0.000;
	// Rdmx
	LR_dmx_coef[0][1] = +0.000; // Lf
	LR_dmx_coef[1][1] = +0.500; // Rf
	LR_dmx_coef[2][1] = +0.354; // C
	LR_dmx_coef[3][1] = +0.177; // LFE
	LR_dmx_coef[4][1] = +0.000; // Ls
	LR_dmx_coef[5][1] = +0.250; // Rs
	LR_dmx_coef[6][1] = +0.000;
	LR_dmx_coef[7][1] = +0.000;
}

void audio_stream_t::set_downmix_coef(double dmx_coef[8][2]) {
	for (int ch = 0; ch < 8; ch++) {
		LR_dmx_coef[ch][0] = dmx_coef[ch][0];
		LR_dmx_coef[ch][1] = dmx_coef[ch][1];
	}
}

void audio_stream_t::downmix_channels(uint8_t* data, int* data_size) {
	int channels = group1_channels + group2_channels;
	int dmx_offset = 0;
	switch (group1_bits) {
	case 16:
		for (int offset = 0; offset < *data_size; offset += channels * sizeof(int16_t)) {
			double L = 0.0;
			double R = 0.0;
			for (int ch = 0; ch < channels; ch++) {
				int16_t sample = *(int16_t*)(data + offset + ch * sizeof(int16_t));
				if (ch < 8) {
					L += (double)sample * LR_dmx_coef[ch][0];
					R += (double)sample * LR_dmx_coef[ch][1];
				}
			}
			*(int16_t*)(data + dmx_offset) = (int16_t)conv_sample(L);
			dmx_offset +=  sizeof(int16_t);
			*(int16_t*)(data + dmx_offset) = (int16_t)conv_sample(R);
			dmx_offset +=  sizeof(int16_t);
		}
		break;
	case 20:
	case 24:
		for (int offset = 0; offset < *data_size; offset += channels * sizeof(int32_t)) {
			double L = 0.0;
			double R = 0.0;
			for (int ch = 0; ch < channels; ch++) {
				if (ch < 8) {
					int32_t sample = *(int32_t*)(data + offset + ch * sizeof(int32_t));
					L += (double)sample * LR_dmx_coef[ch][0];
					R += (double)sample * LR_dmx_coef[ch][1];
				}
			}
			*(int32_t*)(data + dmx_offset) = (int32_t)conv_sample(L);
			dmx_offset +=  sizeof(int32_t);
			*(int32_t*)(data + dmx_offset) = (int32_t)conv_sample(R);
			dmx_offset +=  sizeof(int32_t);
		}
		break;
	default:
		break;
	}
	*data_size = dmx_offset;
}

int mlp_audio_stream_t::truehd_channels(int chanmap) {
	int channels = 0;
	for (int i = 0; i < 13; i++)
		if ((chanmap >> i) & 1)
			for (int j = 0; j < truehd_table[i].channels; j++)
				channels++;
	return channels;
}

audio_stream_info_t* mlp_audio_stream_t::get_info(uint8_t* buf, int buf_size) {
	memset(&avcCtx,       0, sizeof(AVCodecContext));
	memset(&avcParserCtx, 0, sizeof(AVCodecParserContext));
	memset(&mlpParseCtx,  0, sizeof(MLPParseContext));
	memset(&mlpDecodeCtx, 0, sizeof(MLPDecodeContext));
	avcParserCtx.priv_data = (MLPParseContext*)&mlpParseCtx;
	avcCtx.priv_data = (MLPDecodeContext*)&mlpDecodeCtx;
	avcCtx.sample_fmt = SAMPLE_FMT_S16;
	mlpParseCtx.pc.buffer = (uint8_t*)malloc(MLP_PARSE_BUFFER_SIZE);
	mlpParseCtx.pc.buffer_size = MLP_PARSE_BUFFER_SIZE;
	mlp_decoder.init(&avcCtx);
	const uint8_t* out;
	int sync_pos, out_size;
	sync_pos = mlp_parser.parser_parse(&avcParserCtx, &avcCtx, &out, &out_size, buf, buf_size);
	if (!mlpParseCtx.in_sync)
		return 0;
	if (out_size == 0) {
		mlp_parser.parser_parse(&avcParserCtx, &avcCtx, &out, &out_size, buf + sync_pos, buf_size - sync_pos);
		if (!mlpParseCtx.in_sync)
			return 0;
	}
	GetBitContext gb;
	init_get_bits(&gb, buf + sync_pos + 4, (buf_size - sync_pos - 4) * 8);
	if (ff_mlp_read_major_sync(&avcCtx, &mh, &gb) < 0)
		return 0;
	stream_id = UNK_STREAM_ID;
	switch (mh.stream_type) {
	case STREAM_TYPE_MLP:
		stream_type        = STREAM_TYPE_MLP;
		channel_assignment = mh.channels_mlp;
		group1_channels    = mlppcm_table[channel_assignment].group1_channels;
		group1_bits        = mh.group1_bits;
		group1_samplerate  = mh.group1_samplerate;
		group2_channels    = mlppcm_table[channel_assignment].group2_channels;
		group2_bits        = mh.group2_bits;
		group2_samplerate  = mh.group2_samplerate;
		break;
	case STREAM_TYPE_TRUEHD:
		stream_type        = STREAM_TYPE_TRUEHD;
		if (mh.channels_thd_stream2) {
			channel_assignment = mh.channels_thd_stream2;
			group1_channels    = truehd_channels(mh.channels_thd_stream2);
		}
		else {
			channel_assignment = mh.channels_thd_stream1;
			group1_channels    = truehd_channels(mh.channels_thd_stream1);
		}
		group1_bits        = mh.group1_bits;
		group1_samplerate  = mh.group1_samplerate;
		group2_channels    = 0;
		group2_bits        = 0;
		group2_samplerate  = 0;
		break;
	default:
		return 0;
	}
	stream_id          = MLP_STREAM_ID;
	bitrate            = group1_channels * group1_bits * group1_samplerate + group2_channels * group2_bits * group2_samplerate;
	can_downmix        = mh.num_substreams > 1;
	is_vbr             = mh.is_vbr == 1;
	sync_offset        = sync_pos;
	memcpy(&mlp_mh, buf + sync_pos + 4, min(buf_size - sync_pos - 4, sizeof(mlp_mh)));
	return this;
}

int mlp_audio_stream_t::init(uint8_t* buf, int buf_size, bool downmix, bool reset_statistics) {
	if (!get_info(buf, buf_size))
		return -1;
	do_downmix = downmix;
	if (downmix) {
		if (can_downmix)
			avcCtx.request_channels = 2;
		else
			set_downmix_coef();
	}
	if (reset_statistics)
		reset_stats();
	do_check = false;
	return 0;
}

int mlp_audio_stream_t::decode(uint8_t* data, int* data_size, uint8_t* buf, int buf_size) {
	if (do_check && (size_t)buf_size >= sizeof(mlp_mh_t) + 4) {
		mlp_mh_t* buf_mh = (mlp_mh_t*)(buf + 4);
		if ((buf_mh->major_sync & 0xfeffffff) == 0xba6f72f8 && (mlp_mh.major_sync & 0xfeffffff) == 0xba6f72f8) {
			if (
				buf_mh->channel_assignment != mlp_mh.channel_assignment ||
				buf_mh->group1_samplerate  != mlp_mh.group1_samplerate ||
				buf_mh->group1_bits        != mlp_mh.group1_bits ||
				buf_mh->group2_samplerate  != mlp_mh.group2_samplerate ||
				buf_mh->group2_bits        != mlp_mh.group2_bits
			) {
				return RETCODE_REINIT;
			}
		}
	}
	AVPacket avpkt;
	avpkt.data = buf;
	avpkt.size = buf_size;
	int bytes_decoded;
	bytes_decoded = mlp_decoder.decode(&avcCtx, data, data_size, &avpkt);
	if (bytes_decoded > 0) {
		int buf_bits_read = 8 * bytes_decoded;
		int buf_bits_decoded = (*data_size) / (avcCtx.sample_fmt == SAMPLE_FMT_S16 ? 2 : 4) * avcCtx.bits_per_raw_sample;
		if (!do_downmix)
			reorder_channels(data, data_size);
		else if (!can_downmix) {
			reorder_channels(data, data_size);
			downmix_channels(data, data_size);
		}
		if (avcCtx.request_channels > 0)
			buf_bits_decoded = (buf_bits_decoded * (group1_channels + group2_channels)) / avcCtx.channels;
		update_stats(buf_bits_read, buf_bits_decoded);
	}
	return bytes_decoded;
}

int mlp_audio_stream_t::resync(uint8_t* buf, int buf_size) {
	uint32_t major_sync = 0;
	for (int i = 4; i < buf_size; i++) {
		major_sync = (major_sync << 8) | buf[i];
		if ((major_sync & 0xfffffffe) == 0xf8726fba)
			return i - 7;
	}
	return -1;
}

typedef struct {
	uint16_t first_audio_frame;
	uint8_t  padding1;
	uint8_t  group2_bits : 4;
	uint8_t  group1_bits : 4;
	uint8_t  group2_samplerate : 4;
	uint8_t  group1_samplerate : 4;
	uint8_t  padding2;
	uint8_t  channel_assignment;
	uint8_t  padding3;
	uint8_t  cci;
} pcm_header_t;

audio_stream_info_t* pcm_audio_stream_t::get_info(uint8_t* buf, int buf_size) {
	if ((size_t)buf_size < sizeof(pcm_header_t))
		return 0;
	pcm_header_t* ph = (pcm_header_t*)buf;
	if (ph->channel_assignment > 20)
		return 0;
	stream_id          = PCM_STREAM_ID;
	channel_assignment = ph->channel_assignment;
	group1_channels    = mlppcm_table[channel_assignment].group1_channels;
	group2_channels    = mlppcm_table[channel_assignment].group2_channels;
	group1_bits        = ph->group1_bits > 2 ? 0 : 16 + ph->group1_bits * 4;
	group2_bits        = ph->group2_bits > 2 ? 0 : 16 + ph->group2_bits * 4;
	group1_samplerate  = (ph->group1_samplerate & 7) > 2 ? 0 : ph->group1_samplerate & 8 ? 44100 << (ph->group1_samplerate & 7) : 48000 << (ph->group1_samplerate & 7);
	group2_samplerate  = (ph->group2_samplerate & 7) > 2 ? 0 : ph->group2_samplerate & 8 ? 44100 << (ph->group2_samplerate & 7) : 48000 << (ph->group2_samplerate & 7);
	bitrate            = group1_channels * group1_bits * group1_samplerate + group2_channels * group2_bits * group2_samplerate;
	can_downmix        = false;
	is_vbr             = false;
	sync_offset        = 0;
	return this;
}

int pcm_audio_stream_t::init(uint8_t* buf, int buf_size, bool downmix, bool reset_statistics) {
	if (!get_info(buf, buf_size))
		return -1;
	raw_group2_index = 0;
	raw_group2_factor = group2_channels > 0 ? group1_samplerate / group2_samplerate : 1;
	raw_group1_size = group1_channels * group1_bits / 4;
	raw_group2_size = group2_channels * group2_bits / 4;
	pcm_sample_size = group1_bits > 16 ? 4 : 2;
	pcm_group1_size = 2 * group1_channels * pcm_sample_size;
	pcm_group2_size = 2 * group2_channels * pcm_sample_size;
	do_downmix = downmix;
	if (downmix)
		set_downmix_coef();
	if (reset_statistics)
		reset_stats();
	return 0;
}

int pcm_audio_stream_t::decode(uint8_t* data, int* data_size, uint8_t* buf, int buf_size) {
	uint8_t* buf_out = data;
	uint8_t* buf_inp = buf;
	if (buf_size > DVD_BLOCK_SIZE)
		buf_size = DVD_BLOCK_SIZE;
	while (buf_inp + raw_group1_size + (raw_group2_index == 0 ? raw_group2_size : 0) <= buf + buf_size) {
		int pcm_byte_index;
		pcm_byte_index = 0;
		if (raw_group2_index == 0) {
			for (int i = 0; i < 2 * group2_channels; i++) {
				switch (group2_bits) {
				case 16:
					if (group1_bits > 16) {
						pcm_group2_pack[pcm_byte_index++] = 0;
						pcm_group2_pack[pcm_byte_index++] = 0;
					}
					pcm_group2_pack[pcm_byte_index++] = buf_inp[2 * i + 1];
					pcm_group2_pack[pcm_byte_index++] = buf_inp[2 * i];
					break;
				case 20:
					pcm_group2_pack[pcm_byte_index++] = 0;
					if (i % 2)
						pcm_group2_pack[pcm_byte_index++] = buf_inp[4 * group2_channels + i / 2] & 0x0f;
					else
						pcm_group2_pack[pcm_byte_index++] = buf_inp[4 * group2_channels + i / 2] >> 4;
					pcm_group2_pack[pcm_byte_index++] = buf_inp[2 * i + 1];
					pcm_group2_pack[pcm_byte_index++] = buf_inp[2 * i];
					break;
				case 24:
					pcm_group2_pack[pcm_byte_index++] = 0;
					pcm_group2_pack[pcm_byte_index++] = buf_inp[4 * group2_channels + i];
					pcm_group2_pack[pcm_byte_index++] = buf_inp[2 * i + 1];
					pcm_group2_pack[pcm_byte_index++] = buf_inp[2 * i];
					break;
				default:
					break;
				}
			}
			buf_inp += raw_group2_size;
		}
		raw_group2_index++;
		if (raw_group2_index == raw_group2_factor)
			raw_group2_index = 0;
		pcm_byte_index = 0;
		for (int i = 0; i < 2 * group1_channels; i++) {
			switch (group1_bits) {
			case 16:
				pcm_group1_pack[pcm_byte_index++] = buf_inp[2 * i + 1];
				pcm_group1_pack[pcm_byte_index++] = buf_inp[2 * i];
				break;
			case 20:
				pcm_group1_pack[pcm_byte_index++] = 0;
				if (i % 2)
					pcm_group1_pack[pcm_byte_index++] = buf_inp[4 * group1_channels + i / 2] << 4;
				else
					pcm_group1_pack[pcm_byte_index++] = buf_inp[4 * group1_channels + i / 2] & 0xf0;
				pcm_group1_pack[pcm_byte_index++] = buf_inp[2 * i + 1];
				pcm_group1_pack[pcm_byte_index++] = buf_inp[2 * i];
				break;
			case 24:
				pcm_group1_pack[pcm_byte_index++] = 0;
				pcm_group1_pack[pcm_byte_index++] = buf_inp[4 * group1_channels + i];
				pcm_group1_pack[pcm_byte_index++] = buf_inp[2 * i + 1];
				pcm_group1_pack[pcm_byte_index++] = buf_inp[2 * i];
				break;
			default:
				break;
			}
		}
		buf_inp += raw_group1_size;
		memcpy(buf_out, pcm_group1_pack, pcm_group1_size / 2);
		buf_out += pcm_group1_size / 2;
		memcpy(buf_out, pcm_group2_pack, pcm_group2_size / 2);
		buf_out += pcm_group2_size / 2;
		memcpy(buf_out, pcm_group1_pack + pcm_group1_size / 2, pcm_group1_size / 2);
		buf_out += pcm_group1_size / 2;
		memcpy(buf_out, pcm_group2_pack + pcm_group2_size / 2, pcm_group2_size / 2);
		buf_out += pcm_group2_size / 2;
	}
	*data_size = buf_out - data;
	int bytes_decoded = buf_inp - buf;
	int buf_bits_read = 8 * bytes_decoded;
	int buf_samples_decoded = (*data_size) / pcm_sample_size / (group1_channels + group2_channels);
	int buf_bits_decoded = buf_samples_decoded * (group1_channels * group1_bits + group2_channels * group2_bits * group2_samplerate / group1_samplerate);
	if (!do_downmix)
		reorder_channels(data, data_size);
	else if (!can_downmix) {
		reorder_channels(data, data_size);
		downmix_channels(data, data_size);
	}
	update_stats(buf_bits_read, buf_bits_decoded);
	return bytes_decoded;
}

int pcm_audio_stream_t::resync(uint8_t* buf, int buf_size) {
	(void)buf;
	(void)buf_size;
	return 0;
}
