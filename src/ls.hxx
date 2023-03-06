// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_LS_HXX
#define MPD_LS_HXX

#include <stdio.h>

class Response;

/**
 * Checks whether the scheme of the specified URI is supported by MPD.
 * It is not allowed to pass an URI without a scheme, check with
 * uri_has_scheme() first.
 */
[[gnu::pure]]
bool
uri_supported_scheme(const char *url) noexcept;

/**
 * Send a list of supported URI schemes to the client.  This is the
 * response to the "urlhandlers" command.
 */
void print_supported_uri_schemes(Response &r);

/**
 * Send a list of supported URI schemes to a file pointer.
 */
void print_supported_uri_schemes_to_fp(FILE *fp);

#endif
