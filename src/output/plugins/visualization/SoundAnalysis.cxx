// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SoundAnalysis.hxx"

#include "Log.hxx"
#include "config/Block.hxx"
#include "lib/fmt/AudioFormatFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/ThreadIdFormatter.hxx"
#include "pcm/FloatConvert.hxx"
#include "util/Domain.hxx"

#include <cassert>
#include <climits>

const Domain d_sound_analysis("sound_analysis");

Visualization::SoundAnalysisParameters::SoundAnalysisParameters() noexcept
: SoundAnalysisParameters(DEFAULT_NUM_SAMPLES, DEFAULT_LO_CUTOFF, DEFAULT_HI_CUTOFF)
{ }

Visualization::SoundAnalysisParameters::SoundAnalysisParameters(
	const ConfigBlock &config_block)
: SoundAnalysisParameters(
	config_block.GetPositiveValue("num_samples", DEFAULT_NUM_SAMPLES),
	config_block.GetPositiveValue("lo_cutoff", DEFAULT_LO_CUTOFF),
	config_block.GetPositiveValue("hi_cutoff", DEFAULT_HI_CUTOFF))
{ }

Visualization::SoundAnalysisParameters::SoundAnalysisParameters(
	size_t num_samples_in,
	float lo_cutoff_in,
	float hi_cutoff_in):
	num_samples(num_samples_in),
	lo_cutoff(lo_cutoff_in), hi_cutoff(hi_cutoff_in)
{
	if (lo_cutoff >= hi_cutoff) {
		throw FmtRuntimeError(
			"lo_cutoff ({}) must be less than hi_cutoff ({})",
			lo_cutoff, hi_cutoff);
	}
}

/**
 * \page vis_out_dft The Discrete Fourier Transform & Frequency Analysis
 *
 * \section vis_out_dft_intro Introduction
 *
 * This page contains some notes on the Discrete Fourier Transform as applied to
 * music. They are a combination of dimly-remembered mathematics from
 * university, source code comments from the milkdrop Winamp visualization
 * plug-in, and the fftw documentation.
 *
 * \section vis_out_dft_basics The Basics
 *
 * The first thing to note is that the human ear can perceive sounds in the
 * range 200 - 20,000Hz. For visualization purposes, implementations tend to
 * throw away frequency data above 10,000Hz or so since there's not much
 * activity there (something I've observed myself).
 *
 * Perceptually, frequency is not linear, it's logarithmic. A change of one
 * octave corresponds to a doubling in frequency. Intuitively, this means that
 * the difference between, say, 200 & 300Hz is \em much greater than the
 * difference betwen 5000 & 5100Hz, for example.
 *
 * \subsection vis_out_dft_dft The Discrete Fourier Transform
 *
 * Given \c n audio samples, sampled at a frquency of \c F Hz, the DFT computes
 * \c n complex numbers, each of which corresponds to the frequency:
 *
 \code
	    k * F
    freq  = -----,  k=0,...,n-1
	k     n

 \endcode
 *
 * (see
 * <a href="http://fftw.org/fftw3_doc/The-1d-Discrete-Fourier-Transform-_0028DFT_0029.html">here</a>).
 *
 * The DFT library I'm using (<a href="http://fftw.org">fftw</a> AKA "The
 * Fastest Fourier Transform in the West") takes advantage of the Hermitian
 * property of the Fourier Transform of real data in which the k-th Fourier
 * coefficient is the complex conjugate of the the (n-k)-th coefficient and ony
 * returns the first n/2+1 Fourier coefficients (i.e. indicies 0 to n/2,
 * inclusive) to save time & space. See
 * <a href="http://fftw.org/fftw3_doc/The-1d-Real_002ddata-DFT.html">here</a>.
 *
 * Therefore, the first Fourier coefficient returned corresponds to 0Hz, and the
 * last to:
 *
 \code
           n
           - * F
           2       F
    freq = ----- = -
             n     2
 \endcode
 *
 * or half the sampling frequency.
 *
 * \subsection vis_out_dft_buckets How To Bucket Frequencies
 *
 * To divide frequency data into \c N bands (whether for a scheme like bass/
 * mids/trebs, or into a number of bars for visualization purposes), consider
 * your frequency range & compute the number of octaves therein. If we let \c n
 * be the number of octaves, then we know:
 *
 \code
      n     freq_hi        log(freq_hi/freq_lo)
     2  :=  ------- => n = -------------------
            freq_lo            log(2)
 \encode
 *
 * The \c N bands will then be:
 *
 \code
                          n/N
    freq_lo...freq_lo * 2

               n/N             2*n/N
    freq_lo * 2  ...freq_lo * 2

    ...
               (N-1)*n/N           n
    freq_lo * 2  ...    freq_lo * 2

 \endcode
 *
 * \subsection vis_out_dft_eg Worked Example
 *
 * Let the number of samples n be 576. This means our dft will return n/2 + 1 =
 * 289 complex numbers. Let our sampling frequency F be 44,100Hz. For each k,
 * k=0...289, the corresponding frequency will be k * 44100/289, giving us a
 * frequency range of 0Hz to 22,500Hz. Let's clamp that to 200-11,000, compute
 * the power spectrum, and divide that power up into three bands: bass, mids &
 * trebs.
 *
 * First, we need to find the indicies into the dft corresponding to our
 * desired frequency range.
 *
 \code
                       f  * n
         k * F          k
    f  = ----- ==> k = ------, where f := the frequency of the k-th
     k     n              F           k   Fourier coefficient


         | 200 * 576 |
    k0 = | --------- | = floor(2.61...) = 2
         |  44100    |
          -         -

          -           -
         | 11000 * 576 |
    k1 = | ----------- | = ceil(143.67...) = 144
         |   44100     |

 \endcode
 *
 * So the power spectrum will have 144 - 2 + 1 = 143 elements in it. Nb. we're
 * throwing away roughly the upper-half of our frequency spectrum.
 *
 * To divide these frequencies into three bands such that each band contains the
 * same number of octaves, we compute how many octaves there are in our
 * frequency range (call this \c n):
 *
 \code

     n   11000        log(11000/200)
    2  = ----- => n = -------------- = 5.7814
          200            log(2)
 \endcode
 *
 * In other words, there are 5.7814 octaves in our chosen frequency range. We
 * want to give each band 5.7814/3 = 1.9271 octaves. That means the three
 * "buckets" will be:
 *
 \code
                           1.9271
    200 ........... 200 * 2        or   200 -    761Hz

           1.9271         2*1.9271
    200 * 2 ....... 200 * 2        or   761 -  2,892Hz

           2*1.9271        5.7814
    200 * 2 ....... 200 * 2        or 2,892 - 11,000Hz

 \endcode
 *
 *
 */

Visualization::SoundAnalysis::SoundAnalysis(
	const SoundAnalysisParameters &params,
	std::shared_ptr<SoundInfoCache> pc)
: num_samples(params.GetNumSamples()),
  out_samples((num_samples / 2) + 1),
  pcache(pc),
  audio_format(pc->GetFormat()),
  num_channels(audio_format.channels),
  cbbuf(params.GetNumSamples() * audio_format.GetFrameSize()),
  buf(new std::byte[cbbuf]),
  in(fftwf_alloc_real(num_samples * num_channels), fftwf_free),
  out(fftwf_alloc_complex(out_samples * num_channels), fftwf_free),
  bass_mids_trebs(new float[3 * num_channels])
{
	if (num_samples > INT_MAX) {
		throw FmtInvalidArgument(
			"num_samples({}) may not be larger than {}",
			num_samples, INT_MAX);
	}

	int n[] = { (int)num_samples };

	/* The input is assumed to be interleaved; this seems convenient from
	 * the perspective of how it's stored from the AudioOutput... tho if we
	 * need an additional copy to convert it to `float`, we'd have the
	 * opportunity to re-arrange it. */

	int dist = num_samples;

	/* Per the FFTW docs:
	 *
	 * "`rank` is the rank of the transform (it should be the size of the
	 * array `*n`) we use the term rank to denote the number of independent
	 * indices in an array. For example, we say that a 2d transform has rank
	 * 2, a 3d transform has rank 3, and so on."
	 *

	 * This is always 1, for us.
	 *
	 * layout of `in`:
	 *
	 * | 0 ... num_samples-1 | num_samples ... 2*num_samples-1 | 2*num_samples ...
	 * | data for chan 0	 | data for chan 1		   | data for chan 2 */

	/* `howmany` is the number of transforms to compute. The resulting plan
	 * computes `howmany` transforms, where the input of the k-th transform
	 * is at location in+k*idist (in C pointer arithmetic), and its output
	 * is at location out+k*odist. */

	int odist = (num_samples / 2) + 1;

	plan = fftwf_plan_many_dft_r2c(1,             // rank of the input array-- we have one-dimensional arrays
				       n,	      // the number of elements in each array
				       num_channels,  // one array for each channel
				       in.get(),      // input buffer-- need to copy samples here before executing
				       NULL,
				       1,	      // input stride
				       dist,	      // distance between successive arrays (indexes, not bytes)
				       out.get(),     // output buffer-- overwritten on each execution
				       NULL,
				       1,	      // output stride
				       odist,	      // distance between successive arrays (indexes, not bytes)
				       FFTW_ESTIMATE);// should probably be zero (to select a more exhaustive
	// search), but out of an abundance of caution, tell
	// FFTW to pick a plan quickly
	if (NULL == plan) {
		throw FmtRuntimeError("Failed to generate an FFTW plan: "
				      "num_samp={},num_chan={}",
				      num_samples, num_channels);
	}

	freq_lo = params.GetLoCutoff();

	float samples_per_sec = (float) audio_format.GetSampleRate();
	float ns = (float) num_samples;
	// The highest frequency we can represent will be
	float max_freq = ns * samples_per_sec / ns;
	if (max_freq < params.GetHiCutoff()) {
		FmtWarning(d_sound_analysis,
			   "Clamping max frequency from {} to {}",
			   freq_hi, max_freq);
		freq_hi = max_freq;
	} else {
		freq_hi = params.GetHiCutoff();
	}

	idx_lo = (size_t)floorf(freq_lo *
				(float) num_samples / samples_per_sec );
	idx_hi = (size_t) ceilf(freq_hi * (float)num_samples / samples_per_sec);

	float num_octaves = logf(freq_hi/freq_lo) / 0.69314718f;

	float freq_mids = freq_lo * powf(2.0f, num_octaves / 3.0f);
	float freq_trebs = freq_lo * powf(2.0f, 2.0f * num_octaves / 3.0f);

	idx_mids  = ns *  freq_mids / samples_per_sec;
	idx_trebs = ns * freq_trebs / samples_per_sec;
}

bool
Visualization::SoundAnalysis::Update(SoundInfoCache::Time t) noexcept
{
	FmtDebug(d_sound_analysis, "SoundAnalysis::update(tid: {}), time {}us, "
		 "# samp: {}, buffer size: {}", std::this_thread::get_id(),
		 duration_cast<std::chrono::microseconds>(t.time_since_epoch()).count(),
		 num_samples, pcache->Size());

	if (!pcache->GetByTime(num_samples, t, buf.get(), cbbuf)) {
		FmtWarning(d_sound_analysis, "Failed to get samples by time "
			   "for sound analysis ({} samples requested, at "
			   "time {}us for buf size {}).", num_samples,
			   duration_cast<std::chrono::microseconds>(t.time_since_epoch()).count(),
			   cbbuf);
		return false;
	}

	/* Copy the raw PCM data from `buf` into `in`. I hate this, but we need
	 * to convert the input data from `uint16_t` (or whatever) to `float`
	 * regardless. We could, of course, do the conversion when the PCM data
	 * is added to the cache, but since I anticipate processing far fewer
	 * samples than I store, I expect this to be more efficient (both in
	 * terms of time & space).
	 *
	 * Since we have to do the copy anyway, let's convert from interleaved
	 * to sequential (i.e. all samples for the first channel laid-out
	 * contiguously, followed by all from the second, and so forth). */
	typedef IntegerToFloatSampleConvert<SampleFormat::S8> S8Cvt;
	typedef IntegerToFloatSampleConvert<SampleFormat::S16> S16Cvt;
	typedef IntegerToFloatSampleConvert<SampleFormat::S32> S32Cvt;
	typedef IntegerToFloatSampleConvert<SampleFormat::S24_P32> S24P32;

	for (size_t i = 0; i < num_samples; ++i) {
		for (size_t j = 0; j < num_channels; ++j) {
			/* `buf` index: i * num_channels + j
			 * `in` index: j * num_samples + i */
			float x;
			switch (audio_format.format) {
			case SampleFormat::S8:
				x = S8Cvt::Convert(
					*(int8_t*)buf[i * num_channels + j]);
				break;
			case SampleFormat::S16:
				x = S16Cvt::Convert(
					*(int16_t*) (buf.get() +
						     2 * (i*num_channels + j)));
				break;
			case SampleFormat::S32:
				x = S32Cvt::Convert(
					*(int32_t*)(buf.get() +
						    4 * (i*num_channels + j)));
				break;
			case SampleFormat::FLOAT:
				x = *(float*)(buf.get() +
					      4 * (i * num_channels + j));
				break;
			case SampleFormat::S24_P32:
				/* signed 24 bit integer samples, packed in 32
				 * bit integers (the most significant byte is
				 * filled with the sign bit) */
				x = S24P32::Convert(
					*(int32_t *)(buf.get() +
						     4 * (i*num_channels + j)));
				break;
			default:
				assert(false);
			}
			in.get()[j * num_samples + i] = x;
		}
	}

	fftwf_execute(plan);

	size_t max_coeffs_idx = num_samples/2;

	for (unsigned c = 0; c < num_channels; ++c) {

		bass_mids_trebs[3 * c] = bass_mids_trebs[3 * c + 1] =
			bass_mids_trebs[3*c+2] = 0.0f;

		// walk [idx_lo, idx_hi)
		for (size_t i = idx_lo; i < idx_hi; ++i) {
			size_t j = i;
			if (j > max_coeffs_idx) {
				j = num_samples - j;
			}
			fftwf_complex *pout =
				out.get() + c * (max_coeffs_idx + 1);
			float contrib = sqrt(
				pout[j][0]*pout[j][0] + pout[j][1]*pout[j][1]);
			if (i < idx_mids) {
				bass_mids_trebs[3*c] += contrib;
			} else if (i < idx_trebs) {
				bass_mids_trebs[3*c + 1] += contrib;
			} else {
				bass_mids_trebs[3*c + 2] += contrib;
			}
		}
	}

	return true;
}

bool
Visualization::SoundAnalysis::GetCoeffs(fftwf_complex *coeffs,
					size_t num_complex) const noexcept {
	if (num_complex < out_samples * num_channels) {
		return false;
	}

	/* Would prefer to use `std::copy`, but fftw regrettably defines
	 * `fftwf_complex` as `float[2]` which confuses it. */
	memcpy(coeffs, out.get(),
	       out_samples * num_channels * sizeof(fftwf_complex));

	return true;
}

bool
Visualization::SoundAnalysis::GetBassMidsTrebs(float *buf_out,
											   size_t num_buf) const {

	if (num_buf < 3 * num_channels) {
		return false;
	}

	std::copy(bass_mids_trebs.get(),
		  bass_mids_trebs.get() + 3 * num_channels,
		  buf_out);
	return true;
}
