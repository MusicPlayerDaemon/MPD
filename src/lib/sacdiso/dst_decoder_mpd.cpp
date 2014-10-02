/*
* SACD Decoder plugin
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

#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include "stddef.h"

#include <pthread.h>

#include "dst_decoder_mpd.h"

using namespace std;

#define LOG_ERROR ("ERROR: ")
#define LOG(p1, p2, p3)

static void* DSTDecoderThread(void* threadarg) {
	frame_slot_t* frame_slot = (frame_slot_t*)threadarg;
	while (frame_slot->state != SLOT_TERMINATING) {
		sem_wait(&frame_slot->hEventPut);
		frame_slot->state = SLOT_RUNNING;
		Decode(frame_slot->D, frame_slot->dst_data, frame_slot->dsd_data, frame_slot->frame_nr, (int*)&frame_slot->dst_size);
		frame_slot->state = SLOT_READY;
		sem_post(&frame_slot->hEventGet);
	}
	return 0;
}

int dst_decoder_create_mt(dst_decoder_t** dst_decoder, int thread_count) {
	*dst_decoder = (dst_decoder_t*)calloc(1, sizeof(dst_decoder_t));
	if (*dst_decoder == nullptr)	{
		LOG(lm_main, LOG_ERROR, ("Could not create DST decoder object"));
		return -1;
	}
	(*dst_decoder)->frame_slots = (frame_slot_t*)calloc(thread_count, sizeof(frame_slot_t));
	if ((*dst_decoder)->frame_slots == NULL) {
		*dst_decoder = nullptr;
		LOG(lm_main, LOG_ERROR, ("Could not create DST decoder slot array"));
		return -2;
	}
	(*dst_decoder)->thread_count  = thread_count;
	(*dst_decoder)->channel_count = 0;
	(*dst_decoder)->samplerate    = 0;
	(*dst_decoder)->slot_nr       = 0;
	for (int i = 0; i < (*dst_decoder)->thread_count; i++) {
		frame_slot_t* frame_slot = &(*dst_decoder)->frame_slots[i];
		frame_slot->D = (DstDec*)malloc(sizeof(DstDec));
		if (frame_slot->D == nullptr) {
			LOG(lm_main, LOG_ERROR, ("Could not create DST decoder context"));
			return -3;
		}
		sem_init(&frame_slot->hEventPut, 0, 0);
		sem_init(&frame_slot->hEventGet, 0, 0);
		pthread_create(&frame_slot->hThread, nullptr, DSTDecoderThread, (void*)frame_slot);
	}
	return 0;
}

int dst_decoder_destroy_mt(dst_decoder_t* dst_decoder) {
  for (int i = 0; i < dst_decoder->thread_count; i++) {
    frame_slot_t* frame_slot = &dst_decoder->frame_slots[i];
		frame_slot->state = SLOT_TERMINATING;
		if (pthread_cancel(frame_slot->hThread) == 0) {
			pthread_join(frame_slot->hThread, nullptr);
		}
		sem_destroy(&frame_slot->hEventGet);
		sem_destroy(&frame_slot->hEventPut);
		if (frame_slot->D != nullptr) {
			free(frame_slot->D);
			frame_slot->D = nullptr;
		}
  }
  free(dst_decoder->frame_slots);
  free(dst_decoder);
  return 0;
}

int dst_decoder_init_mt(dst_decoder_t* dst_decoder, int channel_count, int samplerate) {
  for (int i = 0; i < dst_decoder->thread_count; i++)	{
    frame_slot_t* frame_slot = &dst_decoder->frame_slots[i];
    if (Init(frame_slot->D, channel_count, samplerate / 44100) == 0) {
      frame_slot->channel_count = channel_count;
      frame_slot->samplerate = samplerate;
      frame_slot->dsd_size = (size_t)(samplerate / 8 / 75 * channel_count);
    }
    else {
      LOG(lm_main, LOG_ERROR, ("Could not initialize decoder slot"));
      return -1;
    }
    frame_slot->initialized = 1;
  }
  dst_decoder->channel_count = channel_count;
  dst_decoder->samplerate = samplerate;
  dst_decoder->frame_nr = 0;
  return 0;
}

int dst_decoder_free_mt(dst_decoder_t* dst_decoder) {
	for (int i = 0; i < dst_decoder->thread_count; i++) {
		uint8_t* dsd_data;
		size_t dsd_size;
		dst_decoder_decode_mt(dst_decoder, nullptr, 0, &dsd_data, &dsd_size);
	}
	for (int i = 0; i < dst_decoder->thread_count; i++) {
		frame_slot_t* frame_slot = &dst_decoder->frame_slots[i];
		if (frame_slot->D != nullptr) {
			if (frame_slot->initialized) {
				if (Close(frame_slot->D) != 0) {
					LOG(lm_main, LOG_ERROR, ("Could not close DST decoder slot"));
				}
				frame_slot->initialized = 0;
			}
		}
	}
	return 0;
}

int dst_decoder_decode_mt(dst_decoder_t* dst_decoder, uint8_t* dst_data, size_t dst_size, uint8_t** dsd_data, size_t* dsd_size) {
  frame_slot_t* frame_slot;

  /* Get current slot */
  frame_slot = &dst_decoder->frame_slots[dst_decoder->slot_nr];

  /* Allocate encoded frame into the slot */
  frame_slot->dsd_data = *dsd_data;
  frame_slot->dst_data = dst_data;
  frame_slot->dst_size = dst_size;
  frame_slot->frame_nr = dst_decoder->frame_nr;

  /* Release worker (decoding) thread on the loaded slot */
  if (dst_size > 0) {
    frame_slot->state = SLOT_LOADED;
    sem_post(&frame_slot->hEventPut);
  }
  else {
    frame_slot->state = SLOT_EMPTY;
  }

  /* Advance to the next slot */
  dst_decoder->slot_nr = (dst_decoder->slot_nr + 1) % dst_decoder->thread_count;
  frame_slot = &dst_decoder->frame_slots[dst_decoder->slot_nr];

  /* Dump decoded frame */
  if (frame_slot->state != SLOT_EMPTY) {
    sem_wait(&frame_slot->hEventGet);
  }
  switch (frame_slot->state) {
  case SLOT_READY:
    *dsd_data = frame_slot->dsd_data;
    *dsd_size = (size_t)(dst_decoder->samplerate / 8 / 75 * dst_decoder->channel_count);
    break;
  case SLOT_READY_WITH_ERROR:
    *dsd_data = frame_slot->dsd_data;
    *dsd_size = (size_t)(dst_decoder->samplerate / 8 / 75 * dst_decoder->channel_count);
    memset(*dsd_data, 0xAA, *dsd_size);
    break;
  default:
    *dsd_data = NULL;
    *dsd_size = 0;
    break;
  }

  dst_decoder->frame_nr++;
  return 0;
}

int dst_decoder_flush_mt(dst_decoder_t* dst_decoder) {
	for (int i = 0; i < dst_decoder->thread_count; i++) {
		uint8_t* dsd_data;
		size_t dsd_size;
		dst_decoder_decode_mt(dst_decoder, nullptr, 0, &dsd_data, &dsd_size);
	}
	return 0;
}
