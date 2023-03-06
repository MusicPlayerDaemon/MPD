// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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
