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

#include "system/Error.hxx"

/* sorry for this horrible piece of code - there's no elegant way to
   load DLLs at runtime */

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

using jack_set_error_function_t = std::add_pointer_t<decltype(jack_set_error_function)>;
static jack_set_error_function_t _jack_set_error_function;

using jack_set_info_function_t = std::add_pointer_t<decltype(jack_set_info_function)>;
static jack_set_info_function_t _jack_set_info_function;

using jack_client_open_t = std::add_pointer_t<decltype(jack_client_open)>;
static jack_client_open_t _jack_client_open;

using jack_client_close_t = std::add_pointer_t<decltype(jack_client_close)>;
static jack_client_close_t _jack_client_close;

using jack_connect_t = std::add_pointer_t<decltype(jack_connect)>;
static jack_connect_t _jack_connect;

using jack_activate_t = std::add_pointer_t<decltype(jack_activate)>;
static jack_activate_t _jack_activate;

using jack_deactivate_t = std::add_pointer_t<decltype(jack_deactivate)>;
static jack_deactivate_t _jack_deactivate;

using jack_get_sample_rate_t = std::add_pointer_t<decltype(jack_get_sample_rate)>;
static jack_get_sample_rate_t _jack_get_sample_rate;

using jack_set_process_callback_t = std::add_pointer_t<decltype(jack_set_process_callback)>;
static jack_set_process_callback_t _jack_set_process_callback;

using jack_on_info_shutdown_t = std::add_pointer_t<decltype(jack_on_info_shutdown)>;
static jack_on_info_shutdown_t _jack_on_info_shutdown;

using jack_free_t = std::add_pointer_t<decltype(jack_free)>;
static jack_free_t _jack_free;

using jack_get_ports_t = std::add_pointer_t<decltype(jack_get_ports)>;
static jack_get_ports_t _jack_get_ports;

using jack_port_register_t = std::add_pointer_t<decltype(jack_port_register)>;
static jack_port_register_t _jack_port_register;

using jack_port_name_t = std::add_pointer_t<decltype(jack_port_name)>;
static jack_port_name_t _jack_port_name;

using jack_port_get_buffer_t = std::add_pointer_t<decltype(jack_port_get_buffer)>;
static jack_port_get_buffer_t _jack_port_get_buffer;

using jack_ringbuffer_create_t = std::add_pointer_t<decltype(jack_ringbuffer_create)>;
static jack_ringbuffer_create_t _jack_ringbuffer_create;

using jack_ringbuffer_free_t = std::add_pointer_t<decltype(jack_ringbuffer_free)>;
static jack_ringbuffer_free_t _jack_ringbuffer_free;

using jack_ringbuffer_get_write_vector_t = std::add_pointer_t<decltype(jack_ringbuffer_get_write_vector)>;
static jack_ringbuffer_get_write_vector_t _jack_ringbuffer_get_write_vector;

using jack_ringbuffer_write_advance_t = std::add_pointer_t<decltype(jack_ringbuffer_write_advance)>;
static jack_ringbuffer_write_advance_t _jack_ringbuffer_write_advance;

using jack_ringbuffer_read_space_t = std::add_pointer_t<decltype(jack_ringbuffer_read_space)>;
static jack_ringbuffer_read_space_t _jack_ringbuffer_read_space;

using jack_ringbuffer_read_t = std::add_pointer_t<decltype(jack_ringbuffer_read)>;
static jack_ringbuffer_read_t _jack_ringbuffer_read;

using jack_ringbuffer_read_advance_t = std::add_pointer_t<decltype(jack_ringbuffer_read_advance)>;
static jack_ringbuffer_read_advance_t _jack_ringbuffer_read_advance;

using jack_ringbuffer_reset_t = std::add_pointer_t<decltype(jack_ringbuffer_reset)>;
static jack_ringbuffer_reset_t _jack_ringbuffer_reset;

template<typename T>
static void
GetFunction(HMODULE h, const char *name, T &result)
{
	auto f = GetProcAddress(h, name);
	if (f == nullptr)
		throw FormatRuntimeError("No such libjack function: %s", name);

	result = reinterpret_cast<T>(f);
}

static void
LoadJackLibrary()
{
#ifdef _WIN64
#define LIBJACK "libjack64"
#else
#define LIBJACK "libjack"
#endif

	auto libjack = LoadLibraryA(LIBJACK);
	if (!libjack)
		throw FormatLastError("Failed to load " LIBJACK ".dll");

	GetFunction(libjack, "jack_set_error_function", _jack_set_error_function);
	GetFunction(libjack, "jack_set_info_function", _jack_set_info_function);

	GetFunction(libjack, "jack_client_open", _jack_client_open);
	GetFunction(libjack, "jack_client_close", _jack_client_close);
	GetFunction(libjack, "jack_connect", _jack_connect);
	GetFunction(libjack, "jack_activate", _jack_activate);
	GetFunction(libjack, "jack_deactivate", _jack_deactivate);
	GetFunction(libjack, "jack_free", _jack_free);

	GetFunction(libjack, "jack_get_sample_rate", _jack_get_sample_rate);
	GetFunction(libjack, "jack_set_process_callback", _jack_set_process_callback);
	GetFunction(libjack, "jack_on_info_shutdown", _jack_on_info_shutdown);

	GetFunction(libjack, "jack_get_ports", _jack_get_ports);
	GetFunction(libjack, "jack_port_register", _jack_port_register);
	GetFunction(libjack, "jack_port_name", _jack_port_name);
	GetFunction(libjack, "jack_port_get_buffer", _jack_port_get_buffer);

	GetFunction(libjack, "jack_ringbuffer_create", _jack_ringbuffer_create);
	GetFunction(libjack, "jack_ringbuffer_free", _jack_ringbuffer_free);
	GetFunction(libjack, "jack_ringbuffer_get_write_vector", _jack_ringbuffer_get_write_vector);
	GetFunction(libjack, "jack_ringbuffer_write_advance", _jack_ringbuffer_write_advance);
	GetFunction(libjack, "jack_ringbuffer_read_space", _jack_ringbuffer_read_space);
	GetFunction(libjack, "jack_ringbuffer_read", _jack_ringbuffer_read);
	GetFunction(libjack, "jack_ringbuffer_read_advance", _jack_ringbuffer_read_advance);
	GetFunction(libjack, "jack_ringbuffer_reset", _jack_ringbuffer_reset);
}

#define jack_set_error_function _jack_set_error_function
#define jack_set_info_function _jack_set_info_function

#define jack_client_open _jack_client_open
#define jack_client_close _jack_client_close
#define jack_connect _jack_connect
#define jack_activate _jack_activate
#define jack_deactivate _jack_deactivate
#define jack_free _jack_free

#define jack_get_sample_rate _jack_get_sample_rate
#define jack_set_process_callback _jack_set_process_callback
#define jack_on_info_shutdown _jack_on_info_shutdown

#define jack_get_ports _jack_get_ports
#define jack_port_register _jack_port_register
#define jack_port_name _jack_port_name
#define jack_port_get_buffer _jack_port_get_buffer

#define jack_ringbuffer_create _jack_ringbuffer_create
#define jack_ringbuffer_free _jack_ringbuffer_free
#define jack_ringbuffer_get_write_vector _jack_ringbuffer_get_write_vector
#define jack_ringbuffer_write_advance _jack_ringbuffer_write_advance
#define jack_ringbuffer_read_space _jack_ringbuffer_read_space
#define jack_ringbuffer_read _jack_ringbuffer_read
#define jack_ringbuffer_read_advance _jack_ringbuffer_read_advance
#define jack_ringbuffer_reset _jack_ringbuffer_reset

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
