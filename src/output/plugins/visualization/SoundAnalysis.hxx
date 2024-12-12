// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef SOUND_ANALYSIS_HXX_INCLUDED
#define SOUND_ANALYSIS_HXX_INCLUDED 1

#include "SoundInfoCache.hxx"
#include "LowLevelProtocol.hxx"

#include <fftw3.h>

#include <algorithm>
#include <cstddef>
#include <memory>

#include <math.h>

struct ConfigBlock;

namespace Visualization {

/**
 * \brief Convenience class for expressing sound analysis parameters exclusive
 * of the audio format
 *
 *
 * There are any number of parameters governing our analysis of PCM data. Other
 * than the `AudioFormat`, they are read from configuration at startup and
 * constant. Rather than force callers to write methods taking many parameters,
 * this class colects them all in one place, and enforces some constraints on
 * their values.
 *
 *
 */

class SoundAnalysisParameters
{
	/* The number of samples used for each analysis; this must be greater
	 * than zero and needn't be large (say, less than 1024). Configuration
	 * value "num_samples" */
	size_t num_samples;
	/* Data lower than this frequency (in the frequency domain) shall be
	 * discarded; must be greater than or equal to zero, and less than
	 * hi_cutoff. A typical value would be 200 (the lower range of human
	 * perception). Units: Hz. Configuration value "lo_cutoff" */
	float lo_cutoff;
	/* Data greater than this frequency (in the frequency domain) shall be
	 * discarded; must be greater than or equal to zero, and greater than
	 * lo_cutoff. A typical value would be 10000-12000 (empirically, there's
	 * not a lot of activity above 10000 in song data). Units
	 * Hz. Configuration value "hi_cutoff" */
	float hi_cutoff;

	static constexpr size_t DEFAULT_NUM_SAMPLES = 513;
	static constexpr size_t DEFAULT_LO_CUTOFF = 200;
	static constexpr size_t DEFAULT_HI_CUTOFF = 10000;

public:
	SoundAnalysisParameters() noexcept;
	explicit SoundAnalysisParameters(const ConfigBlock &config_block);
	SoundAnalysisParameters(size_t num_samples, float lo_cutoff,
				float hi_cutoff);

	size_t
	GetNumSamples() const noexcept {
		return num_samples;
	}
	float
	GetLoCutoff() const noexcept {
		return lo_cutoff;
	}
	float
	GetHiCutoff() const noexcept {
		return hi_cutoff;
	}
};

/**
 * \class SoundAnalysis
 *
 * \brief Analayze PCM data in a manner convienient for visualization authors
 *
 *
 * This class houses our logic for going from raw PCM data to the power
 * spectrum, bass/mids/trebs &c. Instances are constructed with configuration
 * information on the analysis details, and repeated analysis for different
 * datasets is performed via update(). Since instances allocate input & output
 * buffers for the discrete Fourier transform, they are not meant to be copied
 * around.
 *
 *
 */

class SoundAnalysis {

	/// # of samples to be used in each analysis
	size_t num_samples;
	/* # of Fourier coefficients computed by FFTW (should be
	 * (num_samples / 2) + 1) */
	size_t out_samples;
	std::shared_ptr<SoundInfoCache> pcache;
	AudioFormat audio_format;
	/* # of audio channels (e.g. 1 is mono, 2 is stereo--
	 * # cf. SampleFormat.hxx); should be audio_format.num_channels */
	uint8_t num_channels;
	/// Size of `buf`, in bytes
	size_t cbbuf;
	/// Pre-allocated buffer for raw PCM data
	std::unique_ptr<std::byte[]> buf;
	/// Input array for all FFTs performed by this `SoundAnalysis` instance
	std::unique_ptr<float, void (*)(void*)> in;
	/// Output array for all FFTs performed by this `SoundAnalysis` instance
	std::unique_ptr<fftwf_complex, void (*)(void*)> out;
	/* Pre-computed (by fftw) information on the fastest way to compute the
	 * Discrete Fourier Transform on the underlying hardware */
	fftwf_plan plan;
	/* Frequency cutoffs, in Hz; we'll return frequencies in the range
	   [freq_lo, freq_hi] */
	float freq_lo, freq_hi;
	/* Indicies into `out` corresponding to the desired frequency range;
	 * that range is indexed by [index_lo, index_hi) */
	size_t idx_lo, idx_hi;
	/// Indicies into `out` corresponding "mids" & "trebs"
	size_t idx_mids, idx_trebs;
	/* Bass/mids/trebs, laid-out as [bass, mids, trebs, bass, mids, trebs]
	 * (i.e. 3 * num_channels floats) */
	std::unique_ptr<float[]> bass_mids_trebs;

public:
	SoundAnalysis(const SoundAnalysisParameters &params,
		      std::shared_ptr<SoundInfoCache> pc);

	uint8_t
	NumChan() const noexcept {
		return num_channels;
	}
	/// Return the number of audio samples, per channel, used in each analysis
	size_t
	NumSamp() const noexcept {
		return num_samples;
	}
	/* Return the number of Fourier coefficients & power spectrum values
	 * returned, per channel; this is determined by the number of samples and
	 * the frequency cutoffs */
	size_t
	NumFreq() const noexcept {
		return idx_hi - idx_lo;
	}

	/// Update the current analysis to be current as of time \a t
	bool Update(SoundInfoCache::Time t) noexcept;

	/* Return the first half of the Fourier coefficients (bearing in mind
	 * that the Hermitian property means we only need to deal with the first
	 * nsamp/2 + 1) with no frequency cutoffs. Mostly used for testing */
	bool GetCoeffs(fftwf_complex *coeffs,
		       size_t num_complex) const noexcept;
	bool GetBassMidsTrebs(float *buf, size_t num_buf) const;

	/////////////////////////////////////////////////////////////////////////
	//		     Serialization Support                             //
	/////////////////////////////////////////////////////////////////////////

	/* Write the waveforms used in the current analysis to \a pout; return
	 * the updated iterator. The waveforms will be written as per the
	 * \ref vis_out_protocol_proto_frame "protocol spec".
	 */
	template <typename OutIter>
	OutIter
	SerializeWaveforms(OutIter pout) const {
		const float *pin = in.get();
		for (size_t j = 0; j < num_channels; ++j) {
			for (size_t i = 0; i < num_samples; ++i) {
				pout = SerializeFloat(pin[j * num_samples + i],
						      pout);
			}
		}
		return pout;
	}

	/* Write the frequency coefficients that resulted from the current analysis
	 * subject to frequency cutoffs to \a pout; return the updated
	 * iterator. The coefficients will be written as per the
	 * \ref vis_out_protocol_proto_frame "protocol spec". */
	template <typename OutIter>
	OutIter
	SerializeCoefficients(OutIter pout) const {
		return TransformCoeffs(pout,  SerializeComplex);
	}

	/* Write the magnitude of a complex number (presumably a Fourier
	 * coefficient) to \a pout; return the updated iterator. The magnitude will
	 * be written as per the \ref vis_out_protocol_proto_frame "protocol spec". */
	template <typename OutIter>
	static
	OutIter
	SerializeSpectrum(const fftwf_complex c, OutIter pout) {
		return SerializeFloat(sqrt(c[0] * c[0] + c[1] * c[1]), pout);
	}

	/* Write the power spectrum that resulted from the current analysis to \a
	 * pout; return the updated iterator. The power spectrum will be written as
	 * per the \ref vis_out_protocol_proto_frame "protocol spec". */
	template <typename OutIter>
	OutIter
	SerializePowerSpectra(OutIter pout) const {
		return TransformCoeffs(pout, SerializeSpectrum);
	}

	/* Write the bass/mids/trebs values that resulted from the current analysis
	 * to \a pout; return the updated iterator. The values will be written as
	 * per the \ref vis_out_protocol_proto_frame "protocol spec". */
	template <typename OutIter>
	OutIter
	SerializeBassMidsTrebs(OutIter pout) const {
		float *bmt = bass_mids_trebs.get();
		for (size_t i = 0; i < num_channels; ++i) {
			pout = SerializeFloat(bmt[3 * i], pout);
			pout = SerializeFloat(bmt[3 * i + 1], pout);
			pout = SerializeFloat(bmt[3 * i + 2], pout);
		}
		return pout;
	}

	/* Write the payload of a \c FRAME message to \a pout; return the updated
	 * iterator. The payload will be written as per the
	 * \ref vis_out_protocol_proto_frame "protocol spec". */
	template <typename OutIter>
	OutIter
	SerializeSoundInfoFramePayload(OutIter pout) const {
		pout = SerializeU16(num_samples, pout);
		*pout++ = (std::byte) num_channels;
		pout = SerializeU16(audio_format.GetSampleRate(), pout);
		pout = SerializeWaveforms(pout);
		pout = SerializeU16(NumFreq(), pout);
		pout = SerializeFloat(freq_lo, pout);
		pout = SerializeFloat(freq_hi, pout);
		pout = SerializeU16(idx_lo, pout);
		pout = SerializeCoefficients(pout);
		pout = SerializePowerSpectra(pout);
		pout = SerializeBassMidsTrebs(pout);
		return pout;
	}

	/* Write the Fourier coefficients in the range `[idx_lo, idx_hi)` to
	 * \a pout first transforming them by \a op. */
	template <typename OutIter>
	OutIter
	TransformCoeffs(
		OutIter pout,
		OutIter (*op)(const fftwf_complex, OutIter pout)) const {

		/* We wish to serialize the Fourier cofficients [idx_lo,
		 * idx_hi), transformed by `op`. The issue is that `out` stores
		 * the coefficients [0, num_samples/2 + 1), so we need to
		 * transform the indexing operation. */
		const fftwf_complex *po = out.get();

		// The # of frequencies stored in `out` per channel
		size_t total_freq_per_chan = num_samples / 2 + 1;

		// The maximum indexable frequency per channel
		size_t upper_freq_per_chan =
			std::min(idx_hi, total_freq_per_chan);

		/* Control the offset at which we begin indexing into `pout`
		   when copying Fourier coefficients that are the complex
		   conjugates of those actually stored in `po`*/
		size_t second_off = ((num_samples % 2) != 0) ? 1 : 2;
		if (idx_lo > upper_freq_per_chan) {
			second_off += idx_lo - upper_freq_per_chan;
		}

		/* In both `out` & `pout`, the coefficients are laid out as:
		 * | coeffs for chan #0... | coeffs for chan #1... | ... |
		 * so the outer loop will be on channel. */
		for (unsigned chan = 0; chan < num_channels; ++chan) {

			/* This is the index into `out` of the very first
			 * Fourier coefficient for this channel. */
			size_t first_freq_this_chan =
				chan * total_freq_per_chan;
			/* Beginning from here, we wan to walk the indicies:
			 * 	   [idx_lo, upper_freq_per_chan)
			 * This will take us from the "low" frequency index up
			 * to num_samp/2 + 1 or idx_hi, whichever is least. */
			size_t num_to_copy = idx_hi - idx_lo;
			for (size_t i = first_freq_this_chan + idx_lo;
				 i < first_freq_this_chan + upper_freq_per_chan;
			     ++i, --num_to_copy) {
				pout = op(po[i], pout);
			}
			/* *If* idx_hi is greater than num_samp/2+1, walk back
			 * down the Fourier coefficients (taking advantiage of
			 * the Hermetian property). */
			if (idx_hi > total_freq_per_chan) {
				for (size_t i =
					     first_freq_this_chan + total_freq_per_chan
					     - second_off,
					     j = 0;
				     j < num_to_copy;
				     --i, ++j) {
					fftwf_complex c = {
						 po[i][0],
						-po[i][1] };
					pout = op(c, pout);
				}
			}
		}
		return pout;
	}

};

} // namespace Visualization

#endif // SOUND_ANALYSIS_HXX_INCLUDED
