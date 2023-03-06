// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

void
Initialise(JNIEnv *env) noexcept
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
Deinitialise(JNIEnv *env) noexcept
{
	cls.Clear(env);
}

AllocatedPath
getExternalStorageDirectory(JNIEnv *env) noexcept
{
	jobject file =
		env->CallStaticObjectMethod(cls,
					    getExternalStorageDirectory_method);
	if (file == nullptr)
		return nullptr;

	return Java::File::ToAbsolutePath(env, file);
}

AllocatedPath
getExternalStoragePublicDirectory(JNIEnv *env, const char *type) noexcept
{
	if (getExternalStoragePublicDirectory_method == nullptr)
		/* needs API level 8 */
		return nullptr;

	Java::String type2(env, type);
	jobject file = env->CallStaticObjectMethod(cls,
						   getExternalStoragePublicDirectory_method,
						   type2.Get());
	if (file == nullptr)
		return nullptr;

	return Java::File::ToAbsolutePath(env, file);
}

} // namespace Environment
