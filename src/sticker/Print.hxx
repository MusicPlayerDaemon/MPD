// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_STICKER_PRINT_HXX
#define MPD_STICKER_PRINT_HXX

struct Sticker;
class Response;

/**
 * Sends one sticker value to the client.
 */
void
sticker_print_value(Response &r, const char *name, const char *value);

/**
 * Sends all sticker values to the client.
 */
void
sticker_print(Response &r, const Sticker &sticker);

#endif
