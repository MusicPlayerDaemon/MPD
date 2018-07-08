/*
 * Copyright 2003-2018 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** This is the new CoreAudio output plugin which uses direct mode
 *	(without Audio Unit) to output audio. Due to CoreAudio's
 *	inner workings this means that in general all audio is converted
 *	to float sample format. This output uses an audio converter object
 *	to do any conversion required from MPDs input samples to the
 *	format and channels the device asks for. The filter engine of MPD
 *	is therefore only used for the purpose of resampling.
 */


/** TODO:
 *	Callbacks for default device change and reconfiguration
 */

#include "config.h"
#include "MacOSOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "mixer/MixerList.hxx"
#include "util/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "util/Manual.hxx"
#include "util/ConstBuffer.hxx"
#include "pcm/PcmExport.hxx"
#include <AudioToolbox/AudioConverter.h>
#include "Log.hxx"
#include "lib/coreaudio/CoreAudioDevice.hxx"
#include "lib/coreaudio/CoreAudioHelpers.hxx"
#include <boost/lockfree/spsc_queue.hpp>

static constexpr Domain macos_output_domain("macos_output");
// Set output frame buffer to double of the 512 default value
static constexpr unsigned DEFAULT_FRAME_BUFFER_SIZE = 1024;
// Ring buffer of at least 100ms
static constexpr unsigned BUFFER_TIME_MS = 100;

struct MacOSOutput final : AudioOutput {
	
	// Layer around CoreAudio
	CoreAudioDevice device;

	// Plugin settings as fetched from config
	const char *device_name = nullptr;
	std::vector<SInt32> channel_map;
	bool hog_device;
#ifdef ENABLE_DSD
	bool dop_setting;
#endif
	bool integer_mode;
	UInt32 frame_buffer_size;
	
	/** The following are used to do the final
	 *	format conversion before sending the data
	 *	to the audio device. This includes channel
	 *	mapping and (if necessary) de-interleaving
	 *	as well as the mandatory float conversion
	 *	in case integer_mode is not active or not
	 *	supported.
	 */
	AudioConverterRef ca_converter = nullptr;
	AudioBufferList *out_buffer = nullptr;
	// The format MPD sends
	AudioStreamBasicDescription in_format;
	// The format CoreAudio requests for IO
	AudioStreamBasicDescription out_format;

	bool pause;
	
	/** Required to support DoP. No
	 *	other features currently used.
	 */
	Manual<PcmExport> pcm_export;

	boost::lockfree::spsc_queue<uint8_t> *ring_buffer = nullptr;
	UInt32 buffer_ms;
	
	MacOSOutput(const ConfigBlock &block);
	static AudioOutput *Create(EventLoop &, const ConfigBlock &block);

	unsigned GetVolume();
	void SetVolume(unsigned new_volume);

private:
	void Enable() override;
	void Disable() noexcept override;
	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;
	std::chrono::steady_clock::duration Delay() const noexcept override;
	size_t Play(const void *chunk, size_t size) override;
	bool Pause() override;
	void SetupConverter();
	void Setup(AudioFormat &audio_format);
#ifdef ENABLE_DSD
	void SetupDop(AudioFormat audio_format,
				  PcmExport::Params &params);
#endif
	void SetupOrDop(AudioFormat &audio_format, PcmExport::Params &params
#ifdef ENABLE_DSD
					, bool dop
#endif
	);
	static OSStatus RenderCallback(gcc_unused AudioObjectID inDevice, gcc_unused const AudioTimeStamp* inNow, gcc_unused const AudioBufferList* inInputData, gcc_unused const AudioTimeStamp* inInputTime, AudioBufferList* outOutputData, gcc_unused const AudioTimeStamp* inOutputTime, void* inClientData);
};

static bool
macos_output_test_default_device(void)
{
	/** For now return false (not use
	 *	this as default).
	 */
	return false;
}

MacOSOutput::MacOSOutput(const ConfigBlock &block)
		: AudioOutput(FLAG_ENABLE_DISABLE|FLAG_PAUSE)
{
	device_name = block.GetBlockValue("device", "default");
	const char *ch_map = block.GetBlockValue("channel_map");
	if(ch_map != nullptr)
	{
		ParseChannelMap(ch_map, channel_map);
	}
	hog_device = block.GetBlockValue("hog_device", false);
#ifdef ENABLE_DSD
	dop_setting = block.GetBlockValue("dop", false);
#endif
	integer_mode = block.GetBlockValue("integer_mode", false);
	frame_buffer_size = block.GetBlockValue("frame_buffer_size", DEFAULT_FRAME_BUFFER_SIZE);
}

AudioOutput *
MacOSOutput::Create(EventLoop &, const ConfigBlock &block)
{
	MacOSOutput *oo = new MacOSOutput(block);
	return oo;
}

unsigned
MacOSOutput::GetVolume() {
	Float32 vol;
	unsigned volume;
	if(device.HasVolume()) {
		if((vol = device.GetCurrentVolume()) < 0.0) {
			FormatError(macos_output_domain, "Cannot get current volume.");
			volume = 0;
		}
		else
			volume = (unsigned)(vol * 100.0);
	}
	else {
		FormatInfo(macos_output_domain, "The device does not support volume setting.");
		volume = 0;
	}
	return volume;
}

void
MacOSOutput::SetVolume(unsigned new_volume) {
	Float32 vol = (Float32) new_volume / 100.0;
	if(device.HasVolume())
		device.SetCurrentVolume(vol);
	else
		FormatWarning(macos_output_domain, "The device does not support volume setting.");
}

void
MacOSOutput::Enable() {
	device.Open(device_name);
	try {
		FormatDebug(macos_output_domain, "Opened output device: %s", device.GetName());
		device.SetBufferSize(frame_buffer_size);
	}
	catch (...) {
		device.Close();
		throw;
	}
	pcm_export.Construct();
}

void
MacOSOutput::Disable() noexcept {
	device.Close();
	pcm_export.Destruct();
}

void
MacOSOutput::Close() noexcept {
	try {
		device.RemoveIOProc();
		if(hog_device)
			// Release hog mode
			device.SetHogStatus(false);
	}
	catch (...) {
		// Make sure this is really noexcept method.
		FormatDebug(macos_output_domain,"Ignoring exception on close of output.");
	}
	pcm_export->Reset();
	DeallocateABL(out_buffer);
	out_buffer = nullptr;
	AudioConverterDispose(ca_converter);
	ca_converter = nullptr;
	delete ring_buffer;
}

void
MacOSOutput::SetupConverter() {
	/** Setup the audio converter in the following cases:
	 *	1. Integer mode is not used and float conversion is needed
	 *	2. Number of channels for MPD format and device format differs
	 *	3. Channel map was specified and therefore mapping / re-ordering required
	 *	4. Usage of planar audio device (de-interleaving required)
	 */
	OSStatus err = noErr;
	if(!channel_map.empty()) {
		if(out_format.mChannelsPerFrame > channel_map.size())
			throw FormatRuntimeError("Channel map contains only %l channels, output device requires %d channels.", channel_map.size(), out_format.mChannelsPerFrame);
		err = AudioConverterNew(&in_format, &out_format, &ca_converter);
		/** Pass directly the array indicating with the size parameter
		 *	the number of channels to be read from the channel_map.
		 */
		err = AudioConverterSetProperty(ca_converter, kAudioConverterChannelMap, out_format.mChannelsPerFrame * sizeof(SInt32), channel_map.data());

	}
	else if(!(out_format.mFormatFlags & kAudioFormatFlagIsNonMixable) || device.IsPlanar() || in_format.mChannelsPerFrame != out_format.mChannelsPerFrame) {
		// Integer mode not active, planar device or channel conversion needed
		err = AudioConverterNew(&in_format, &out_format, &ca_converter);
	}
	
	if(err != noErr)
		throw std::runtime_error("Failed to setup AudioConverter for MacOS output.");
	
	/** Allocate the buffer used for output conversion.
	 *	Since the CoreAudio HAL asks as max for each
	 *	callback for the number of frames that are set
	 *	as the (variable) device buffer size, exactly
	 *	this amount gets allocated here.
	 */
	try {
		if(ca_converter != nullptr)
			out_buffer = AllocateABL(in_format, device.GetBufferSize());
	}
	catch (...) {
		AudioConverterDispose(ca_converter);
		ca_converter = nullptr;
		throw;
	}
}

void
MacOSOutput::Setup(AudioFormat &audio_format) {
	if (device.SetFormat(audio_format, integer_mode)) {
		/** Report back the actual physical device format
		 *	to make sure that MPDs output engine sends
		 *	the physical format. This will get transformed
		 *	to the virtual format in final conversion
		 *	directly in the render callback (to float
		 *	samples in case integer_mode is not
		 *	configured or not supported).
		 */
		
		memset(&in_format, 0, sizeof(in_format));
		memset(&out_format, 0, sizeof(out_format));
		
		in_format = device.GetPhysFormat();
		out_format = device.GetIOFormat();
		AudioFormat phys_format = ASBDToAudioFormat(in_format);
		audio_format.format = phys_format.format;
		audio_format.sample_rate = phys_format.sample_rate;
		// Adjust converter input format accordingly to MPD format
		in_format = AudioFormatToASBD(audio_format);
		FormatDebug(macos_output_domain, "Sending format %s to output device.", ToString(audio_format).c_str());
	}
	else
		throw std::runtime_error("Unable to set output format for MacOS output.");
	
}

#ifdef ENABLE_DSD

void
MacOSOutput::SetupDop(const AudioFormat audio_format, PcmExport::Params &params) {
	assert(audio_format.format == SampleFormat::DSD);
	
	/* pass 24 bit to Setup() */
	
	AudioFormat dop_format = audio_format;
	dop_format.format = SampleFormat::S24_P32;
	dop_format.sample_rate = params.CalcOutputSampleRate(audio_format.sample_rate);
	
	const AudioFormat check = dop_format;
	
	Setup(dop_format);
	
	/* if the device allows only 32 bit, shift all DoP
	 samples left by 8 bit and leave the lower 8 bit cleared;
	 the DSD-over-USB documentation does not specify whether
	 this is legal, but there is anecdotical evidence that this
	 is possible (and the only option for some devices) */
	params.shift8 = dop_format.format == SampleFormat::S32;
	
	if (dop_format.format == SampleFormat::S32)
		dop_format.format = SampleFormat::S24_P32;
	
	if (dop_format != check)
		/* no bit-perfect playback, which is required
		 for DSD over USB */
		throw std::runtime_error("Failed to configure DSD-over-PCM, no suitable format available.");
}

#endif

void
MacOSOutput::SetupOrDop(AudioFormat &audio_format, PcmExport::Params &params
#ifdef ENABLE_DSD
					   , bool dop
#endif
) {
#ifdef ENABLE_DSD
	std::exception_ptr dop_error;
	if(audio_format.format == SampleFormat::DSD) {
		if (dop) {
			try {
				params.dop = true;
				SetupDop(audio_format, params);
				return;
			}
			catch (...) {
				dop_error = std::current_exception();
				// DoP was unsuccessful, proceed with PCM output
				params.dop = false;
				audio_format.format = SampleFormat::S32;
			}
		}
		else
			/** If DoP is not configured switch to
			 *	PCM output (DSD direct not possible
			 *	on MacOS).
			 */
			audio_format.format = SampleFormat::S32;
	}
	
	try {
#endif
		Setup(audio_format);
#ifdef ENABLE_DSD
	}
	catch (...) {
		if (dop_error)
			/* if DoP was attempted, prefer returning the
			 original DoP error instead of the fallback
			 error */
			std::rethrow_exception(dop_error);
		else
			throw;
	}
#endif
}

void
MacOSOutput::Open(AudioFormat &audio_format) {
	PcmExport::Params params;
	
#ifdef ENABLE_DSD
	bool dop = dop_setting;
	params.dop = false;
#endif
	
	try {
		SetupOrDop(audio_format, params
#ifdef ENABLE_DSD
				   , dop
#endif
				   );
	}
	catch (...) {
		std::throw_with_nested(FormatRuntimeError("Error opening MacOS output device \"%s\"", device_name));
	}
	
#ifdef ENABLE_DSD
	if (params.dop)
		FormatDebug(macos_output_domain, "DoP (DSD over PCM) enabled");
#endif
	
	// Setup converter used to transform MPD's format to the CoreAudio IO format
	SetupConverter();
	
	if(hog_device)
		device.SetHogStatus(true);
	
	/** Setup the ring_buffer to hold
	 *	BUFFER_TIME_MS or four times
	 *	the device frame buffer.
	 */
	UInt32 dev_frame_buffer = device.GetBufferSize();
	size_t ring_buffer_size = std::max(4 * dev_frame_buffer * in_format.mBytesPerFrame, BUFFER_TIME_MS * (unsigned int) in_format.mBytesPerFrame * (unsigned int) in_format.mSampleRate / 1000);
	buffer_ms = ring_buffer_size / (in_format.mSampleRate * in_format.mBytesPerFrame) * 1000;
	FormatDebug(macos_output_domain, "Using buffer size of %d ms and %ld bytes", buffer_ms, ring_buffer_size);
	ring_buffer = new boost::lockfree::spsc_queue<uint8_t>(ring_buffer_size);
	
	pcm_export->Open(audio_format.format, audio_format.channels, params);
	
	// Register for data request callbacks from the driver and start
	device.AddIOProc(RenderCallback, this);
	pause = false;
}

size_t
MacOSOutput::Play(const void *chunk, size_t size) {
	assert(size > 0);
	
	if(pause) {
		pause = false;
		device.Start();
	}
	const auto e = pcm_export->Export({chunk, size});
	if (e.size == 0)
		/** The DoP (DSD over PCM) filter converts two frames
		 *	at a time and ignores the last odd frame; if there
		 *	was only one frame (e.g. the last frame in the
		 *	file), the result is empty; to avoid an endless
		 *	loop, bail out here, and pretend the one frame has
		 *	been played
		 */
		return size;

	size_t bytes_written = ring_buffer->push((const uint8_t *)e.data, e.size);
	return pcm_export->CalcSourceSize(bytes_written);
}

std::chrono::steady_clock::duration
MacOSOutput::Delay() const noexcept {
	if(pause)
		return std::chrono::seconds(1);
	// Wait for half the buffer size in case the buffer is full
	return ring_buffer->write_available() ? std::chrono::steady_clock::duration::zero() : std::chrono::milliseconds(buffer_ms / 2);
}
	
bool
MacOSOutput::Pause() {
	if(!pause) {
		pause = true;
		device.Stop();
	}
	return true;
}
	
int
macos_output_get_volume(MacOSOutput &output) {
	return output.GetVolume();
}

void
macos_output_set_volume(MacOSOutput &output, unsigned new_volume) {
	return output.SetVolume(new_volume);
}
	
OSStatus
MacOSOutput::RenderCallback(gcc_unused AudioObjectID inDevice, gcc_unused const AudioTimeStamp* inNow, gcc_unused const AudioBufferList* inInputData, gcc_unused const AudioTimeStamp* inInputTime, AudioBufferList* outOutputData, gcc_unused const AudioTimeStamp* inOutputTime, void* inClientData) {
	MacOSOutput *oo = (MacOSOutput*)inClientData;
	UInt32 requested = outOutputData->mBuffers[oo->device.GetStreamIdx()].mDataByteSize;
	/** Frames are the same for both MPD input format and CoreAudio
	 *	output format as the sample rates are matching.
	 */
	UInt32 frames = requested / oo->out_format.mBytesPerFrame;
	// Number of bytes to pop from ring_buffer (MPD format framesize times number of frames)
	UInt32 in_bytes = std::min(frames * oo->in_format.mBytesPerFrame, (UInt32) oo->ring_buffer->read_available());
	UInt32 available_frames = in_bytes / oo->in_format.mBytesPerFrame;
	
	if(available_frames < frames)
		FormatDebug(macos_output_domain, "Frames available (%d) less than requested (%d) by device.", available_frames, frames);
	
	// Usage of converter
	if(oo->ca_converter != nullptr) {
		// Copy data to the interleaved buffer that was setup as input for the converter
		oo->ring_buffer->pop((uint8_t *)oo->out_buffer->mBuffers[0].mData, in_bytes);
		oo->out_buffer->mBuffers[0].mDataByteSize = in_bytes;
		
		if(oo->device.IsPlanar()) {
			/** For a planar device (several output streams with exactly one channel)
			 *	use ConvertComplexBuffer to directly convert MPD's interleaved
			 *	data to separate channel buffers.
			 */
			AudioConverterConvertComplexBuffer(oo->ca_converter, frames, oo->out_buffer, outOutputData);
		}
		else {
			UInt32 written;
			AudioConverterConvertBuffer(oo->ca_converter, in_bytes, oo->out_buffer->mBuffers[0].mData, &written, outOutputData->mBuffers[oo->device.GetStreamIdx()].mData);
			
		}
	}
	// Direct copy
	else {
		// Copy data to the interleaved buffer of the output device
		oo->ring_buffer->pop((uint8_t *)outOutputData->mBuffers[oo->device.GetStreamIdx()].mData, in_bytes);
	}
	return noErr;
}
	
const struct AudioOutputPlugin macos_output_plugin = {
	"macos",
	macos_output_test_default_device,
	&MacOSOutput::Create,
	&macos_mixer_plugin,
};
