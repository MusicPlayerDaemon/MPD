// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

class OutputStream;
class Encoder;

/**
 * Read all available output from the #Encoder and write it to the
 * #OutputStream.
 *
 * Throws on error.
 */
void
EncoderToOutputStream(OutputStream &os, Encoder &encoder);
