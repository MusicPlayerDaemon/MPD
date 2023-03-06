// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
	assert(type != nullptr);

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
