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

#include "Environment.hxx"
#include "java/Class.hxx"
#include "java/String.hxx"
#include "java/File.hxx"
#include "util/StringUtil.hxx"
#include "fs/AllocatedPath.hxx"

namespace Environment {
	static Java::TrivialClass cls;
	static jmethodID getExternalStorageDirectory_method;
	static jmethodID getExternalStoragePublicDirectory_method;
}

void
Environment::Initialise(JNIEnv *env) noexcept
{
	cls.Find(env, "android/os/Environment");

	getExternalStorageDirectory_method =
		env->GetStaticMethodID(cls, "getExternalStorageDirectory",
				       "()Ljava/io/File;");

	getExternalStoragePublicDirectory_method =
		env->GetStaticMethodID(cls, "getExternalStoragePublicDirectory",
				       "(Ljava/lang/String;)Ljava/io/File;");
}

void
Environment::Deinitialise(JNIEnv *env) noexcept
{
	cls.Clear(env);
}

AllocatedPath
Environment::getExternalStorageDirectory() noexcept
{
	JNIEnv *env = Java::GetEnv();

	jobject file =
		env->CallStaticObjectMethod(cls,
					    getExternalStorageDirectory_method);
	if (file == nullptr)
		return nullptr;

	return Java::File::ToAbsolutePath(env, file);
}

AllocatedPath
Environment::getExternalStoragePublicDirectory(const char *type) noexcept
{
	if (getExternalStoragePublicDirectory_method == nullptr)
		/* needs API level 8 */
		return nullptr;

	JNIEnv *env = Java::GetEnv();

	Java::String type2(env, type);
	jobject file = env->CallStaticObjectMethod(Environment::cls,
						   Environment::getExternalStoragePublicDirectory_method,
						   type2.Get());
	if (file == nullptr)
		return nullptr;

	return Java::File::ToAbsolutePath(env, file);
}
