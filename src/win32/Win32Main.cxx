/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "Main.hxx"

#ifdef _WIN32

#include "util/Compiler.h"
#include "Instance.hxx"
#include "system/FatalError.hxx"

#include <cstdlib>
#include <atomic>

#include <windows.h>
#include <tchar.h>

static int service_argc;
static char **service_argv;
static TCHAR service_name[] = _T("");
static std::atomic_bool running;
static SERVICE_STATUS_HANDLE service_handle;

static void WINAPI
service_main(DWORD argc, LPTSTR argv[]);

static constexpr SERVICE_TABLE_ENTRY service_registry[] = {
	{service_name, service_main},
	{nullptr, nullptr}
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
service_dispatcher(gcc_unused DWORD control, gcc_unused DWORD event_type,
		   gcc_unused void *event_data, gcc_unused void *context)
{
	switch (control) {
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		global_instance->Break();
		return NO_ERROR;
	default:
		return NO_ERROR;
	}
}

static void WINAPI
service_main(gcc_unused DWORD argc, gcc_unused LPTSTR argv[])
{
	service_handle =
		RegisterServiceCtrlHandlerEx(service_name,
					     service_dispatcher, nullptr);

	if (service_handle == 0)
		FatalSystemError("RegisterServiceCtrlHandlerEx() failed");

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
		if (running.load()) {
			// Recent msdn docs that process is terminated
			// if this function returns TRUE.
			// We initiate correct shutdown sequence (if possible).
			// Once main() returns CRT will terminate our process
			// regardless our thread is still active.
			// If this did not happen within 3 seconds
			// let's shutdown anyway.
			global_instance->Break();
			// Under debugger it's better to wait indefinitely
			// to allow debugging of shutdown code.
			Sleep(IsDebuggerPresent() ? INFINITE : 3000);
		}
		// If we're not running main loop there is no chance for
		// clean shutdown.
		std::exit(EXIT_FAILURE);
		return TRUE;
	default:
		return FALSE;
	}
}

int win32_main(int argc, char *argv[])
{
	service_argc = argc;
	service_argv = argv;

	if (StartServiceCtrlDispatcher(service_registry))
		return 0; /* run as service successefully */

	const DWORD error_code = GetLastError();
	if (error_code == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
		/* running as console app */
		running.store(false);
		SetConsoleTitle(_T("Music Player Daemon"));
		SetConsoleCtrlHandler(console_handler, TRUE);
		return mpd_main(argc, argv);
	}

	FatalSystemError("StartServiceCtrlDispatcher() failed", error_code);
}

void win32_app_started()
{
	if (service_handle != 0)
		service_notify_status(SERVICE_RUNNING);
	else
		running.store(true);
}

void win32_app_stopping()
{
	if (service_handle != 0)
		service_notify_status(SERVICE_STOP_PENDING);
	else
		running.store(false);
}

#endif
