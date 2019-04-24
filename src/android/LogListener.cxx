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

#include "LogListener.hxx"
#include "java/Class.hxx"
#include "java/String.hxx"
#include "util/AllocatedString.hxx"
#include "util/FormatString.hxx"

void
LogListener::OnLog(JNIEnv *env, int priority,
		   const char *fmt, ...) const noexcept
{
	assert(env != nullptr);

	Java::Class cls(env, env->GetObjectClass(Get()));

	jmethodID method = env->GetMethodID(cls, "onLog",
					    "(ILjava/lang/String;)V");

	assert(method);

	va_list args;
	va_start(args, fmt);
	const auto log = FormatStringV(fmt, args);
	va_end(args);

	env->CallVoidMethod(Get(), method, priority,
			    Java::String(env, log.c_str()).Get());
}
