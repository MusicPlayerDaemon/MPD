/*
 * Copyright 2010-2018 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
	assert(env != nullptr);
	assert(_file != nullptr);

	LocalObject file(env, _file);

	const jstring path = GetAbsolutePath(env, file);
	if (DiscardException(env) || path == nullptr)
		return nullptr;

	Java::String path2(env, path);
	return AllocatedPath::FromFS(path2.ToString());
}
