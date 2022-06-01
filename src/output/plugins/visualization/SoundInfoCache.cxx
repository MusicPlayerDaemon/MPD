// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SoundInfoCache.hxx"

#include "Log.hxx"
#include "lib/fmt/ThreadIdFormatter.hxx"
#include "util/Domain.hxx"

#include <cmath>
#include <cstring>

using namespace Visualization;
using namespace std::chrono;

const Domain d_sound_info_cache("vis_sound_info_cache");

inline
typename std::chrono::microseconds::rep
NowTicks() {
	return duration_cast<std::chrono::microseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

Visualization::SoundInfoCache::SoundInfoCache(const AudioFormat &audio_format,
					      const Duration &buf_span):
	fmt(audio_format),
	secs_per_frame(1. / double(fmt.GetSampleRate())),
	frame_size(audio_format.GetFrameSize()),
	ring(fmt.TimeToSize(buf_span)),
	cb(0),
	p0(0),
	p1(0)
{ }

/**
 * \brief Add \a size bytes of PCM data to the cache; \a data is assumed to be
 * PCM data in our audio format
 *
 *
 * \param data [in] Address of a buffer containing PCM samples to be added to the cache
 *
 * \param size [in] Size of \a data, in bytes
 *
 *
 * This method will add \a data to the end of the cache, overwriting earlier
 * data if necessary.
 *
 * Nb. regarding the corner case where \a size is larger than the cache itself:
 * in this event, the implementation will simply write as much of \a data into
 * the cache as possible, discarding both the first portion of \a data as well
 * as the previous contents of the cache.
 *
 *
 */

void
Visualization::SoundInfoCache::Add(const void *data, size_t size)
{
	FmtDebug(d_sound_info_cache, "[{}] SoundInfoCache::add(tid:{},"
		  "bytes:{})", NowTicks(), std::this_thread::get_id(), size);

	std::lock_guard<Mutex> guard(mutex);

	if (t0.time_since_epoch().count() == 0) {
		t0 = system_clock::now();
		t1 = t0;
	}

	size_t cb_ring = ring.size();
	if (size > cb_ring) {
		/* Special case: we can't fit this chunk into the ring buffer;
		   just take the last `cb_ring` bytes & discard everything
		   earlier. */
		size_t lost = size - cb_ring;
		memcpy(ring.data(), (const uint8_t*)data + lost, cb_ring);
		cb = cb_ring;
		p0 = p1 = 0;
		t1 += fmt.SizeToTime<Duration>(size);
		t0 = t1 - fmt.SizeToTime<Duration>(cb_ring);
	} else {
		/* Happy path: `size` is <= `cb_ring`. We can fit it all, but
		   may overwrite old data. */
		size_t part1 =
			std::min(size, cb_ring - p1); // #bytes written at p1
		size_t part2 = size - part1; // #bytes "wrapped around"

		memcpy(ring.data() + p1, data, part1);
		memcpy(ring.data(), (const uint8_t*)data + part1, part2);

		p1 = (p1 + size) % cb_ring;

		// # bytes overwritten at start/p0
		size_t part3;
		if (cb == cb_ring) {
			part3 = size;
		} else {
			part3 = part2 > (size_t) p0 ? part2 - p0 : 0;
		}

		p0 = (p0 + part3) % cb_ring;
		cb = cb + size - part3;

		t0 += fmt.SizeToTime<Duration>(part3);
		t1 += fmt.SizeToTime<Duration>(size);
	}
}

// This is primarily used for testing purposes.
bool
Visualization::SoundInfoCache::GetFromBeginning(size_t nsamp,
						void *buf,
						size_t cbbuf) const
{
	std::lock_guard<Mutex> guard(mutex);

	size_t cbsamp = nsamp * frame_size;
	if (cbsamp > cbbuf) {
		return false;
	}

	size_t part1 = std::min(cbsamp, ring.size() - p0);
	size_t part2 = cbsamp - part1;
	memcpy(buf, ring.data() + p0, part1);
	memcpy((uint8_t*)buf + part1, ring.data(), part2);

	return true;
}

/**
 * \brief Retrieve \a nsamp PCM samples ending at time \a t; copy them into
 * \a buf; will return false if this cannot be done for any reason
 *
 *
 * \param nsamp [in] the number of PCM samples desired by the caller; this
 * corresponds to an AudioFormat "frame": IOW each sample is made up of multiple
 * channels of PCM data
 *
 * \param t [in] the time at which the sampling shall \e end
 *
 * \param buf [in] a caller-supplied buffer to which, on success, \a nsamp
 * audio frames will be copied
 *
 * \param cbbuf [in] the size, in bytes, of the buffer at \a buf
 *
 * \return true on success, false on failure
 *
 *
 * This method will copy \a nsamp audio samples ending at time \a t into
 * \a buf. If \a t does not exactly correspond to an audio sample, it will be
 * adjusted by the implementation to correspond to the next whole sample.
 *
 *
 */

bool
Visualization::SoundInfoCache::GetByTime(size_t nsamp, Time t,
					 void *buf, size_t cbbuf) const
{
	using std::min;

	FmtDebug(d_sound_info_cache, "[{}] SoundInfoCache::get_by_time"
		 "(tid:{},t:{}us, delta:{}us)", NowTicks(), std::this_thread::get_id(),
		 duration_cast<std::chrono::microseconds>(t.time_since_epoch()).count(),
		 duration_cast<std::chrono::microseconds>(t1 - t).count());

	std::lock_guard<Mutex> guard(mutex);

	size_t cbsamp = nsamp * frame_size;
	if (cbsamp > cbbuf) {
		/* Can't fit the requested number of frames/samples into `buf`--
		 fail. */
		FmtWarning(d_sound_info_cache,
			   "[{}] SoundInfoCache::get_by_time: can't fit {} "
			   "samples into {} bytes", NowTicks(), nsamp, cbbuf);
		return false;
	}

	if (t > t1) {
		FmtWarning(d_sound_info_cache,
			   "[{}] SoundInfoCache::get_by_time: time t {}us is "
			   "greater than time t1 {}us-- failing.",
			   NowTicks(),
			   duration_cast<std::chrono::microseconds>(t.time_since_epoch()).count(),
			   duration_cast<std::chrono::microseconds>(t1.time_since_epoch()).count());
		return false;
	}

	/* Determine which frame `t` falls into. If `t - t0` is a perfect
	   multiple of the time-per-frame, use the last frame.

	   I need the duration in `t-t0` to be in seconds, but in seconds, but
	   with the fractional part. */
	double delta_t = double(duration_cast<microseconds>(t - t0).count()) / 1000000.;
	ptrdiff_t pb =
		p0 + ptrdiff_t(ceil(delta_t / secs_per_frame)) * frame_size;

	// Make sure we have enough samples in [t0, t) to satisfy this request.
	size_t cb_in_buf = size_t(ceil(delta_t / secs_per_frame)) * frame_size;
	if (cbsamp > cb_in_buf) {
		FmtWarning(d_sound_info_cache,
			   "[{}] SoundInfoCache::get_by_time: the requested "
			   "number of samples take up {} bytes, but we only "
			   "have {} bytes in the buffer.",
			   NowTicks(), cbsamp, cb_in_buf);
		return false;
	}

	size_t cb_ring = ring.size();
	ptrdiff_t pa = pb - nsamp * frame_size;
	pb = pb % cb_ring;
	pa = pa % cb_ring;

	/* So we want to copy offsets [pa, pb) % cb_ring :=> buf. "part1"
	   denotes the range from `pa` to the end of the buffer, and "part2"
	   that from the start of the buffer to `pb`. */
	size_t part1 = min(cbsamp, cb_ring - pa);
	size_t part2 = cbsamp - part1;
	memcpy(buf, ring.data() + pa, part1);
	memcpy((uint8_t*)buf + part1, ring.data(), part2);

	return true;
}

/// Return true IFF the ring buffer is empty
bool
Visualization::SoundInfoCache::Empty() const {
	std::lock_guard<Mutex> guard(mutex);
	return 0 == Size();
}

std::pair<SoundInfoCache::Time, SoundInfoCache::Time>
Visualization::SoundInfoCache::Range() const
{
	std::lock_guard<Mutex> guard(mutex);
	return std::make_pair(t0, t1);
}

std::size_t
Visualization::SoundInfoCache::Size() const
{
	std::lock_guard<Mutex> guard(mutex);
	return cb;
}
