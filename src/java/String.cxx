// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "String.hxx"
#include "util/TruncateString.hxx"
#include "util/ScopeExit.hxx"

Java::String::String(JNIEnv *_env, std::string_view _value) noexcept
	// TODO: is there no way to do this without duplicating the string?
	:String(_env, std::string{_value}.c_str())
{
}

char *
Java::String::CopyTo(JNIEnv *env, jstring value,
		     char *buffer, size_t max_size) noexcept
{
	return CopyTruncateString(buffer, GetUTFChars(env, value).c_str(),
				  max_size);
}

std::string
Java::String::ToString(JNIEnv *env, jstring s) noexcept
{
	assert(env != nullptr);
	assert(s != nullptr);

	return std::string(GetUTFChars(env, s).c_str());
}
