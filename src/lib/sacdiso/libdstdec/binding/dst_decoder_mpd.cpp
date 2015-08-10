/*
* MPD SACD Decoder plugin
* Copyright (c) 2014-2015 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
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

#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include "stddef.h"

#include <pthread.h>

#include "dst_decoder_mpd.h"

using namespace std;

#define DSD_SILENCE_BYTE 0x69

#define LOG_ERROR ("ERROR: ")
#define LOG(p1, p2)

static void* DSTDecoderThread(void* threadarg) {
	frame_slot_t* slot = (frame_slot_t*)threadarg;
	while (slot->run_slot) {
		sem_wait(&slot->hEventPut);
		if (slot->run_slot) {
			slot->state = SLOT_RUNNING;
			slot->D.decode(slot->dst_data, slot->dst_size * 8, slot->dsd_data);
			slot->state = SLOT_READY;
		}
		else {
			slot->dsd_data = nullptr;
			slot->dst_size = 0;
		}
		sem_post(&slot->hEventGet);
	}
	return 0;
}

dst_decoder_t::dst_decoder_t(int threads) {
	thread_count = threads;
	frame_slots = new frame_slot_t[thread_count];
	if (!frame_slots) {
		thread_count = 0;
		LOG(LOG_ERROR, ("Could not create DST decoder slot array"));
	}
	channel_count = 0;
	samplerate    = 0;
	framerate     = 0;
	slot_nr       = 0;
}

dst_decoder_t::~dst_decoder_t() {
	for (int i = 0; i < thread_count; i++) {
		frame_slot_t* slot = &frame_slots[i];
		slot->state = SLOT_TERMINATING;
		slot->D.close();
		slot->run_slot = false;
		sem_post(&slot->hEventPut); // Release worker (decoding) thread for exit
		// Wait until worker (decoding) thread exit
		if (pthread_cancel(slot->hThread) == 0) {
			pthread_join(slot->hThread, nullptr);
		}
		sem_destroy(&slot->hEventGet);
		sem_destroy(&slot->hEventPut);
	}
	delete[] frame_slots;
}

int dst_decoder_t::get_slot_nr() {
	return slot_nr;
}

int dst_decoder_t::init(int _channel_count, int _samplerate, int _framerate) {
	channel_count = _channel_count;
	samplerate = _samplerate;
	framerate = _framerate;
	frame_nr = 0;
	for (int i = 0; i < thread_count; i++)	{
		frame_slot_t* slot = &frame_slots[i];
		if (slot->D.init(channel_count, (samplerate / 44100) / (framerate / 75)) == 0) {
			slot->channel_count = channel_count;
			slot->samplerate = samplerate;
			slot->framerate = framerate;
			slot->dsd_size = (size_t)(samplerate / 8 / framerate * channel_count);
			slot->run_slot = true;
			sem_init(&slot->hEventGet, 0, 0);
			sem_init(&slot->hEventPut, 0, 0);
			pthread_create(&slot->hThread, nullptr, DSTDecoderThread, (void*)slot);
		}
		else {
			LOG(LOG_ERROR, ("Could not initialize decoder slot"));
			return -1;
		}
	}
	return 0;
}

int dst_decoder_t::decode(uint8_t* dst_data, size_t dst_size, uint8_t** dsd_data, size_t* dsd_size) {
	frame_slot_t* slot;

	/* Get current slot */
	slot = &frame_slots[slot_nr];

	/* Allocate encoded frame into the slot */
	slot->dsd_data = *dsd_data;
	slot->dst_data = dst_data;
	slot->dst_size = dst_size;
	slot->frame_nr = frame_nr;

	/* Release worker (decoding) thread on the loaded slot */
	if (dst_size > 0)	{
		slot->state = SLOT_LOADED;
		sem_post(&slot->hEventPut);
	}
	else {
		slot->state = SLOT_EMPTY;
	}

	/* Advance to the next slot */
	slot_nr = (slot_nr + 1) % thread_count;
	slot = &frame_slots[slot_nr];

	/* Dump decoded frame */
	if (slot->state != SLOT_EMPTY) {
		sem_wait(&slot->hEventGet);
	}
	switch (slot->state) {
	case SLOT_READY:
		*dsd_data = slot->dsd_data;
		*dsd_size = (size_t)(samplerate / 8 / framerate * channel_count);
		break;
	case SLOT_READY_WITH_ERROR:
		*dsd_data = slot->dsd_data;
		*dsd_size = (size_t)(samplerate / 8 / framerate * channel_count);
		memset(*dsd_data, DSD_SILENCE_BYTE, *dsd_size);
		break;
	default:
		*dsd_data = nullptr;
		*dsd_size = 0;
		break;
	}
	frame_nr++;
	return 0;
}
