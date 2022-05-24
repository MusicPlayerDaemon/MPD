/*
 * Copyright 2019-2022 Max Kellermann <max.kellermann@gmail.com>
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

#include "Base64.hxx"
#include "lib/ffmpeg/Error.hxx"
#include "util/AllocatedArray.hxx"

extern "C" {
#include <libavutil/base64.h>
}

#include <string>

size_t
DecodeBase64(std::span<std::byte> out, std::string_view in)
{
	/* since av_base64_decode() wants a null-terminated string, we
	   need to make a copy here and null-terminate it */
	const std::string copy{in};
	return DecodeBase64(out, copy.c_str());
}

size_t
DecodeBase64(std::span<std::byte> out, const char *in)
{
	int nbytes = av_base64_decode((uint8_t *)out.data(), in, out.size());
	if (nbytes < 0)
		throw MakeFfmpegError(nbytes, "Base64 decoder failed");

	return nbytes;
}

AllocatedArray<std::byte>
DecodeBase64(std::string_view src)
{
	AllocatedArray<std::byte> dest{CalculateBase64OutputSize(src.size())};
	const std::size_t dest_size = DecodeBase64(dest, src);
	dest.SetSize(dest_size);
	return dest;
}
