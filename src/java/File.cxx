// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "File.hxx"
#include "Class.hxx"
#include "String.hxx"
#include "Object.hxx"
#include "Exception.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Limits.hxx"

jmethodID Java::File::getAbsolutePath_method;

void
Java::File::Initialise(JNIEnv *env) noexcept
{
	Class cls(env, "java/io/File");

	getAbsolutePath_method = env->GetMethodID(cls, "getAbsolutePath",
						  "()Ljava/lang/String;");
}

AllocatedPath
Java::File::ToAbsolutePath(JNIEnv *env, jobject _file) noexcept
{
	LocalObject file(env, _file);

	const jstring path = GetAbsolutePath(env, file);
	if (DiscardException(env) || path == nullptr)
		return nullptr;

	Java::String path2(env, path);
	return AllocatedPath::FromFS(path2.ToString());
}
