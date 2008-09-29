/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

/*
 * Some of the most boring code in the world that I didn't want cluttering
 * up outputBuffer.c
 */

unsigned long ob_get_elapsed_time(void)
{
	return (unsigned long)(ob.elapsed_time + 0.5);
}

unsigned long ob_get_total_time(void)
{
	return (unsigned long)(ob.total_time + 0.5);
}

unsigned int ob_get_bit_rate(void)
{
	return (unsigned int)ob.bit_rate;
}

unsigned int ob_get_channels(void)
{
	return (unsigned int)ob.audio_format.channels;
}

unsigned int ob_get_sample_rate(void)
{
	return (unsigned int)ob.audio_format.sampleRate;
}

unsigned int ob_get_bits(void)
{
	return (unsigned int)ob.audio_format.bits;
}

void ob_set_sw_volume(int volume)
{
	ob.sw_vol = (volume > 1000) ? 1000 : (volume < 0 ? 0 : volume);
}

void ob_set_xfade(float xfade_sec)
{
	ob.xfade_time = (xfade_sec < 0) ? 0 : xfade_sec;
}

float ob_get_xfade(void)
{
	return ob.xfade_time;
}

enum ob_state ob_get_state(void)
{
	return ob.state;
}

AudioFormat *ob_audio_format(void)
{
	return &ob.audio_format;
}

uint8_t ob_get_decoder_sequence(void)
{
	return ob.seq_decoder;
}

uint8_t ob_get_player_sequence(void)
{
	return ob.seq_player;
}

