/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#include "Context.hxx"
#include "java/Class.hxx"
#include "java/Exception.hxx"
#include "java/File.hxx"
#include "java/String.hxx"
#include "fs/AllocatedPath.hxx"

#include "AudioManager.hxx"

static jmethodID getExternalFilesDir_method,
  getCacheDir_method,
  getSystemService_method;

void
Context::Initialise(JNIEnv *env) noexcept
{
	Java::Class cls{env, "android/content/Context"};

	getExternalFilesDir_method = env->GetMethodID(cls, "getExternalFilesDir",
						      "(Ljava/lang/String;)Ljava/io/File;");
	getCacheDir_method = env->GetMethodID(cls, "getCacheDir",
					      "()Ljava/io/File;");
	getSystemService_method = env->GetMethodID(cls, "getSystemService",
						   "(Ljava/lang/String;)Ljava/lang/Object;");
}

AllocatedPath
Context::GetExternalFilesDir(JNIEnv *env, const char *type) noexcept
{
	assert(_type != nullptr);

	jobject file = env->CallObjectMethod(Get(), getExternalFilesDir_method,
					     Java::String::Optional(env, type).Get());
	if (Java::DiscardException(env) || file == nullptr)
		return nullptr;

	return Java::File::ToAbsolutePath(env, file);
}

AllocatedPath
Context::GetCacheDir(JNIEnv *env) const noexcept
{
	assert(env != nullptr);

	jobject file = env->CallObjectMethod(Get(), getCacheDir_method);
	if (Java::DiscardException(env) || file == nullptr)
		return nullptr;

	return Java::File::ToAbsolutePath(env, file);
}

AudioManager *
Context::GetAudioManager(JNIEnv *env) noexcept
{
	assert(env != nullptr);

	Java::String name(env, "audio");
	jobject am = env->CallObjectMethod(Get(), getSystemService_method, name.Get());
	if (Java::DiscardException(env) || am == nullptr)
		return nullptr;

    return new AudioManager(env, am);
}
