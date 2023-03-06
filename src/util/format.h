// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPC_FORMAT_H
#define MPC_FORMAT_H

#include "Compiler.h"

struct mpd_song;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Pretty-print an object into a string using the given format
 * specification.
 *
 * @param format the format string
 * @param object the object
 * @param getter a getter function that extracts a value from the object
 * @return the resulting string to be freed by free(); NULL if
 * no format string group produced any output
 */
gcc_malloc
char *
format_object(const char *format, const void *object,
	      const char *(*getter)(const void *object, const char *name));

#ifdef __cplusplus
}
#endif

#endif
