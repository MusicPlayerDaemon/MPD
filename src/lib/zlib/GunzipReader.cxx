// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "GunzipReader.hxx"
#include "Error.hxx"

GunzipReader::GunzipReader(Reader &_next)
	:next(_next)
{
	z.next_in = nullptr;
	z.avail_in = 0;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;

	int result = inflateInit2(&z, 16 + MAX_WBITS);
	if (result != Z_OK)
		throw ZlibError(result);
}

inline bool
GunzipReader::FillBuffer()
{
	auto w = buffer.Write();
	assert(!w.empty());

	std::size_t nbytes = next.Read(w);
	if (nbytes == 0)
		return false;

	buffer.Append(nbytes);
	return true;
}

std::size_t
GunzipReader::Read(std::span<std::byte> dest)
{
	if (eof)
		return 0;

	z.next_out = (Bytef *)dest.data();
	z.avail_out = dest.size();

	while (true) {
		int flush = Z_NO_FLUSH;

		auto r = buffer.Read();
		if (r.empty()) {
			if (FillBuffer())
				r = buffer.Read();
			else
				flush = Z_FINISH;
		}

		z.next_in = reinterpret_cast<Bytef *>(r.data());
		z.avail_in = r.size();

		int result = inflate(&z, flush);
		if (result == Z_STREAM_END) {
			eof = true;
			return dest.size() - z.avail_out;
		} else if (result != Z_OK)
			throw ZlibError(result);

		buffer.Consume(r.size() - z.avail_in);

		if (z.avail_out < dest.size())
			return dest.size() - z.avail_out;
	}
}
