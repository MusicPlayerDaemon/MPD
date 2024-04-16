// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef SOUND_INFO_CACHE_HXX_INCLUDED
#define SOUND_INFO_CACHE_HXX_INCLUDED

#include "output/Timer.hxx"
#include "pcm/AudioFormat.hxx"
#include "thread/Mutex.hxx"
#include "util/AllocatedArray.hxx"

#include <chrono>
#include <memory>

namespace Visualization {

/**
 * \brief Thread-safe cache for recent PCM data
 *
 *
 * Class SoundInfoCache maintains a ring buffer (AKA circular buffer) for PCM
 * data to cap the amount of of memory used. It keeps two pointers into that
 * buffer: the beginning and the end of valid data, along with the timestamps
 * corresponding to each.
 *
 * The general contract is that once the ctor returns, the caller has an
 * instance with an empty ring buffer & that is ready to accept data. Time
 * starts from the first invocation of add(). Successive invocations of add()
 * are assumed to represent contiguous ranges of sound data (i.e. there is no
 * way to represent gaps).
 *
 * Instances may have their methods invoked by multiple threads, so any method
 * invocation will block on acquiring a Mutex. I had initially considered a
 * single-writer, multi-reader lock in the interests of allowing many
 * simultaneous reads, but in practice it would not be an improvement, since
 * there is only one reader & one writer, and the writer, empirically, is the
 * more frequent caller.
 *
 * A circular buffer is surprisingly difficult to write. I considered
 * abstracting this implementation into a general purpose library class, but
 * there are a number of implementation-specific choices arguing against that:
 *
 * - using a flag versus wasting a slot to distinguish full from empty
 * - overwrite versus drop when new data won't fit
 * - copy in bulk (via `mempcy()`) versus copying slot-by-slot
 *
 * In the end I decided to just write an application-specific implementation.
 *
 *
 */

class SoundInfoCache {
public:
        typedef std::chrono::system_clock::duration Duration;
        typedef std::chrono::time_point<std::chrono::system_clock> Time;

private:
	AudioFormat fmt;
	/// Time per frame, in seconds
	double secs_per_frame;
	/// Sample size, in bytes
	unsigned frame_size;
	/* Mutex guarding the ring buffer since instances will be accessed from
	   multiple threads */
	mutable Mutex mutex;
	/// this is the ring buffer
        AllocatedArray<uint8_t> ring;
	/// # of bytes currently in the ring buffer (as distinct from capacity)
	std::size_t cb;
	/// Valid PCM data exists in buf[p0, p1)
	size_t p0, p1;
	/// Time t0 corresponds to p0, t1 to p1
	Time t0, t1;

public:
	/* Create a cache storing \a buf_span time's worth PCM data in format
	   \a audio_format */
	SoundInfoCache(const AudioFormat &audio_format,
		       const Duration &buf_span);

public:
	/* Add \a size bytes of PCM data to the cache; \a data is assumed to be
	   PCM data in our audio format */
	void Add(const void *data, size_t size);
	AudioFormat GetFormat() const noexcept {
		return fmt;
	}
	/* Read \a nsamp audio samples from the \e beginning of the buffer; will
	   return false if \a buf is not large enough to accomodate that */
	bool GetFromBeginning(size_t nsamp, void *buf, size_t cbbuf) const;
	/* Retrieve \a nsamp PCM samples ending at time \a t; copy them into \a
	   buf; will return false if this cannot be done for any reason */
	bool GetByTime(size_t nsamp, Time t, void *buf, size_t cbbuf) const;
	/// Return true IFF the ring buffer is empty
	bool Empty() const;
	/// Retrieve the time range for which this cache has data
	std::pair<Time, Time> Range() const;
	/// Return the # of bytes in the buffer (as opposed to buffer capacity)
	std::size_t Size() const;
};

} // namespace Visualization

#endif // SOUND_INFO_CACHE_HXX_INCLUDED
