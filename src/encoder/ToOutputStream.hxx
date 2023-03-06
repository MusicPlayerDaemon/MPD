// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ENCODER_TO_OUTPUT_STREAM_HXX
#define MPD_ENCODER_TO_OUTPUT_STREAM_HXX

class OutputStream;
class Encoder;

void
EncoderToOutputStream(OutputStream &os, Encoder &encoder);

#endif
