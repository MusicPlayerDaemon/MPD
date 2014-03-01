/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

namespace Environment {
	static Java::TrivialClass cls;
	static jmethodID getExternalStorageDirectory_method;
	static jmethodID getExternalStoragePublicDirectory_method;

	static jstring getExternalStorageDirectory(JNIEnv *env);
};

void
Environment::Initialise(JNIEnv *env)
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
Environment::Deinitialise(JNIEnv *env)
{
	cls.Clear(env);
}

static jstring
ToAbsolutePathChecked(JNIEnv *env, jobject file)
{
	if (file == nullptr)
		return nullptr;

	jstring path = Java::File::getAbsolutePath(env, file);
	env->DeleteLocalRef(file);
	return path;
}

static jstring
Environment::getExternalStorageDirectory(JNIEnv *env)
{
	jobject file = env->CallStaticObjectMethod(cls,
						   getExternalStorageDirectory_method);
	return ToAbsolutePathChecked(env, file);
}

char *
Environment::getExternalStorageDirectory(char *buffer, size_t max_size)
{
	JNIEnv *env = Java::GetEnv();

	jstring value = getExternalStorageDirectory(env);
	if (value == nullptr)
		return nullptr;

	Java::String value2(env, value);
	value2.CopyTo(env, buffer, max_size);
	return buffer;
}

static jstring
getExternalStoragePublicDirectory(JNIEnv *env, const char *type)
{
	if (Environment::getExternalStoragePublicDirectory_method == nullptr)
		/* needs API level 8 */
		return nullptr;

	Java::String type2(env, type);
	jobject file = env->CallStaticObjectMethod(Environment::cls,
						   Environment::getExternalStoragePublicDirectory_method,
						   type2.Get());
	return ToAbsolutePathChecked(env, file);
}

char *
Environment::getExternalStoragePublicDirectory(char *buffer, size_t max_size,
					       const char *type)
{
	JNIEnv *env = Java::GetEnv();
	jstring path = ::getExternalStoragePublicDirectory(env, type);
	if (path == nullptr)
		return nullptr;

	Java::String path2(env, path);
	path2.CopyTo(env, buffer, max_size);
	return buffer;
}
