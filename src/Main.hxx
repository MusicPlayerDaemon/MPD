/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_MAIN_HXX
#define MPD_MAIN_HXX

struct Instance;

#ifdef ANDROID
#include "android/LogListener.hxx"

extern class Context *context;
extern LogListener *logListener;
#endif

extern Instance *global_instance;

#ifndef ANDROID

/**
 * A entry point for application.
 * On non-Windows platforms this is called directly from main()
 * On Windows platform this is called from win32_main()
 * after doing some initialization.
 */
int
mpd_main(int argc, char *argv[]);

#endif

#ifdef _WIN32

/**
 * If program is run as windows service performs nessesary initialization
 * and then calls mpd_main() with specified arguments.
 * If program is run as a regular application calls mpd_main() immediately.
 */
int
win32_main(int argc, char *argv[]);

/**
 * When running as a service reports to service control manager
 * that our service is started.
 * When running as a console application enables console handler that will
 * trigger GlobalEvents::SHUTDOWN when user closes console window
 * or presses Ctrl+C.
 * This function should be called just before entering main loop.
 */
void
win32_app_started();

/**
 * When running as a service reports to service control manager
 * that our service is about to stop.
 * When running as a console application enables console handler that will
 * catch all shutdown requests and ignore them.
 * This function should be called just after leaving main loop.
 */
void
win32_app_stopping();

#endif

#endif
