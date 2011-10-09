/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#include "config.h"
#include "main.h"

#ifdef WIN32

#include "mpd_error.h"
#include "event_pipe.h"

#include <glib.h>

#include <windows.h>

static int service_argc;
static char **service_argv;
static char service_name[] = "";
static BOOL ignore_console_events;
static SERVICE_STATUS_HANDLE service_handle;

static void WINAPI
service_main(DWORD argc, CHAR *argv[]);

static SERVICE_TABLE_ENTRY service_registry[] = {
	{service_name, service_main},
	{NULL, NULL}
};

static void
service_notify_status(DWORD status_code)
{
	SERVICE_STATUS current_status;

	current_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	current_status.dwControlsAccepted = status_code == SERVICE_START_PENDING
		? 0
		: SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP;

	current_status.dwCurrentState = status_code;
	current_status.dwWin32ExitCode = NO_ERROR;
	current_status.dwCheckPoint = 0;
	current_status.dwWaitHint = 1000;

	SetServiceStatus(service_handle, &current_status);
}

static DWORD WINAPI
service_dispatcher(G_GNUC_UNUSED DWORD control, G_GNUC_UNUSED DWORD event_type,
		   G_GNUC_UNUSED void *event_data, G_GNUC_UNUSED void *context)
{
	switch (control) {
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		event_pipe_emit(PIPE_EVENT_SHUTDOWN);
		return NO_ERROR;
	default:
		return NO_ERROR;
	}
}

static void WINAPI
service_main(G_GNUC_UNUSED DWORD argc, G_GNUC_UNUSED CHAR *argv[])
{
	DWORD error_code;
	gchar* error_message;

	service_handle =
		RegisterServiceCtrlHandlerEx(service_name,
					     service_dispatcher, NULL);

	if (service_handle == 0) {
		error_code = GetLastError();
		error_message = g_win32_error_message(error_code);
		MPD_ERROR("RegisterServiceCtrlHandlerEx() failed: %s",
			  error_message);
	}

	service_notify_status(SERVICE_START_PENDING);
	mpd_main(service_argc, service_argv);
	service_notify_status(SERVICE_STOPPED);
}

static BOOL WINAPI
console_handler(DWORD event)
{
	switch (event) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
		if (!ignore_console_events)
			event_pipe_emit(PIPE_EVENT_SHUTDOWN);
		return TRUE;
	default:
		return FALSE;
	}
}

int win32_main(int argc, char *argv[])
{
	DWORD error_code;
	gchar* error_message;

	service_argc = argc;
	service_argv = argv;

	if (StartServiceCtrlDispatcher(service_registry))
		return 0; /* run as service successefully */

	error_code = GetLastError();
	if (error_code == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
		/* running as console app */
		SetConsoleTitle("Music Player Daemon");
		ignore_console_events = TRUE;
		SetConsoleCtrlHandler(console_handler, TRUE);
		return mpd_main(argc, argv);
	}

	error_message = g_win32_error_message(error_code);
	MPD_ERROR("StartServiceCtrlDispatcher() failed: %s", error_message);
}

void win32_app_started()
{
	if (service_handle != 0)
		service_notify_status(SERVICE_RUNNING);
	else
		ignore_console_events = FALSE;
}

void win32_app_stopping()
{
	if (service_handle != 0)
		service_notify_status(SERVICE_STOP_PENDING);
	else
		ignore_console_events = TRUE;
}

#endif
