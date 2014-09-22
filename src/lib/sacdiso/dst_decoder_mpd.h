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

#ifndef _DST_DECODER_H_INCLUDED
#define _DST_DECODER_H_INCLUDED

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>

#include "libdstdec/dst_decoder.h"

enum slot_state_t {SLOT_EMPTY, SLOT_LOADED, SLOT_RUNNING, SLOT_READY, SLOT_READY_WITH_ERROR, SLOT_TERMINATING};

typedef struct _frame_slot_t {
  volatile int state;
  int          initialized;
  int          frame_nr;
  uint8_t*     dsd_data;
	size_t       dsd_size;
  uint8_t*     dst_data;
	size_t       dst_size;
  int          channel_count;
  int          samplerate;
  pthread_t    hThread;
  sem_t        hEventGet;
  sem_t        hEventPut;
  DstDec*      D;
} frame_slot_t;

typedef struct _dst_decoder_t {
  frame_slot_t* frame_slots;
  int           slot_nr;
  int           thread_count;
  int           channel_count;
  int           samplerate;
  int           frame_nr;
} dst_decoder_t;

int dst_decoder_create_mt(dst_decoder_t** dst_decoder, int thread_count);
int dst_decoder_destroy_mt(dst_decoder_t* dst_decoder);
int dst_decoder_init_mt(dst_decoder_t* dst_decoder, int channel_count, int samplerate);
int dst_decoder_free_mt(dst_decoder_t* dst_decoder);
int dst_decoder_decode_mt(dst_decoder_t* dst_decoder, uint8_t* dst_data, size_t dst_size, uint8_t** dsd_data, size_t* dsd_size);
int dst_decoder_flush_mt(dst_decoder_t* dst_decoder);

#endif
