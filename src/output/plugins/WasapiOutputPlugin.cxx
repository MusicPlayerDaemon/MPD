/*
 * Copyright 2020 The Music Player Daemon Project
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
#include <initguid.h>

#include "Log.hxx"
#include "WasapiOutputPlugin.hxx"
#include "lib/icu/Win32.hxx"
#include "mixer/MixerList.hxx"
#include "thread/Cond.hxx"
#include "thread/Mutex.hxx"
#include "thread/Thread.hxx"
#include "util/AllocatedString.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "util/ScopeExit.hxx"
#include "win32/Com.hxx"
#include "win32/ComHeapPtr.hxx"
#include "win32/HResult.hxx"
#include "win32/WinEvent.hxx"

#include <algorithm>
#include <boost/lockfree/spsc_queue.hpp>
#include <cinttypes>
#include <cmath>
#include <functiondiscoverykeys_devpkey.h>
#include <optional>
#include <variant>

namespace {
static constexpr Domain wasapi_output_domain("wasapi_output");

gcc_const constexpr uint32_t GetChannelMask(const uint8_t channels) noexcept {
	switch (channels) {
	case 1:
		return KSAUDIO_SPEAKER_MONO;
	case 2:
		return KSAUDIO_SPEAKER_STEREO;
	case 3:
		return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER;
	case 4:
		return KSAUDIO_SPEAKER_QUAD;
	case 5:
		return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER |
		       SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
	case 6:
		return KSAUDIO_SPEAKER_5POINT1;
	case 7:
		return KSAUDIO_SPEAKER_5POINT1 | SPEAKER_BACK_CENTER;
	case 8:
		return KSAUDIO_SPEAKER_7POINT1_SURROUND;
	default:
		gcc_unreachable();
	}
}

template <typename Functor>
inline bool SafeTry(Functor &&functor) {
	try {
		functor();
		return true;
	} catch (std::runtime_error &err) {
		FormatError(wasapi_output_domain, "%s", err.what());
		return false;
	}
}

template <typename Functor>
inline bool SafeSilenceTry(Functor &&functor) {
	try {
		functor();
		return true;
	} catch (std::runtime_error &err) {
		return false;
	}
}

inline void SetFormat(WAVEFORMATEXTENSIBLE &device_format,
		      const AudioFormat &audio_format) noexcept {
	device_format.dwChannelMask = GetChannelMask(audio_format.channels);
	device_format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	device_format.Format.nChannels = audio_format.channels;
	device_format.Format.nSamplesPerSec = audio_format.sample_rate;
	device_format.Format.nBlockAlign = audio_format.GetFrameSize();
	device_format.Format.nAvgBytesPerSec =
		audio_format.sample_rate * audio_format.GetFrameSize();
	device_format.Format.wBitsPerSample = audio_format.GetSampleSize() * 8;
	device_format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
	device_format.Samples.wValidBitsPerSample = audio_format.GetSampleSize() * 8;
	if (audio_format.format == SampleFormat::FLOAT) {
		device_format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
	} else {
		device_format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
	}
}

inline constexpr const unsigned int kErrorId = -1;

} // namespace

class WasapiOutputThread : public Thread {
public:
	enum class Status : uint32_t { FINISH, PLAY, PAUSE };
	WasapiOutputThread(std::shared_ptr<WinEvent> _event, ComPtr<IAudioClient> _client,
			   ComPtr<IAudioRenderClient> &&_render_client,
			   const UINT32 _frame_size, const UINT32 _buffer_size_in_frames,
			   bool _is_exclusive,
			   boost::lockfree::spsc_queue<BYTE> &_spsc_buffer)
	: Thread(BIND_THIS_METHOD(Work)), event(std::move(_event)),
	  client(std::move(_client)), render_client(std::move(_render_client)),
	  frame_size(_frame_size), buffer_size_in_frames(_buffer_size_in_frames),
	  is_exclusive(_is_exclusive), spsc_buffer(_spsc_buffer) {}
	void Finish() noexcept { return SetStatus(Status::FINISH); }
	void Play() noexcept { return SetStatus(Status::PLAY); }
	void Pause() noexcept { return SetStatus(Status::PAUSE); }
	void WaitWrite() noexcept {
		std::unique_lock<Mutex> lock(write.mutex);
		write.cond.wait(lock);
	}
	void CheckException() {
		std::unique_lock<Mutex> lock(error.mutex);
		if (error.error_ptr) {
			std::exception_ptr err = std::exchange(error.error_ptr, nullptr);
			error.cond.notify_all();
			std::rethrow_exception(err);
		}
	}

private:
	std::shared_ptr<WinEvent> event;
	std::optional<COM> com;
	ComPtr<IAudioClient> client;
	ComPtr<IAudioRenderClient> render_client;
	const UINT32 frame_size;
	const UINT32 buffer_size_in_frames;
	bool is_exclusive;
	boost::lockfree::spsc_queue<BYTE> &spsc_buffer;
	alignas(BOOST_LOCKFREE_CACHELINE_BYTES) std::atomic<Status> status =
		Status::PAUSE;
	alignas(BOOST_LOCKFREE_CACHELINE_BYTES) struct {
		Mutex mutex;
		Cond cond;
	} write{};
	alignas(BOOST_LOCKFREE_CACHELINE_BYTES) struct {
		Mutex mutex;
		Cond cond;
		std::exception_ptr error_ptr = nullptr;
	} error{};

	void SetStatus(Status s) noexcept {
		status.store(s);
		event->Set();
	}
	void Work() noexcept;
};

class WasapiOutput final : public AudioOutput {
public:
	static AudioOutput *Create(EventLoop &, const ConfigBlock &block);
	WasapiOutput(const ConfigBlock &block);
	void Enable() override;
	void Disable() noexcept override;
	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;
	std::chrono::steady_clock::duration Delay() const noexcept override;
	size_t Play(const void *chunk, size_t size) override;
	void Drain() override;
	bool Pause() override;

	constexpr bool Exclusive() const { return is_exclusive; }
	constexpr size_t FrameSize() const { return device_format.Format.nBlockAlign; }
	constexpr size_t SampleRate() const {
		return device_format.Format.nSamplesPerSec;
	}

private:
	bool is_started = false;
	bool is_exclusive;
	bool enumerate_devices;
	std::string device_config;
	std::vector<std::pair<unsigned int, AllocatedString<char>>> device_desc;
	std::shared_ptr<WinEvent> event;
	std::optional<COM> com;
	ComPtr<IMMDeviceEnumerator> enumerator;
	ComPtr<IMMDevice> device;
	ComPtr<IAudioClient> client;
	WAVEFORMATEXTENSIBLE device_format;
	std::unique_ptr<WasapiOutputThread> thread;
	std::unique_ptr<boost::lockfree::spsc_queue<BYTE>> spsc_buffer;
	std::size_t watermark;

	friend bool wasapi_is_exclusive(WasapiOutput &output) noexcept;
	friend IMMDevice *wasapi_output_get_device(WasapiOutput &output) noexcept;
	friend IAudioClient *wasapi_output_get_client(WasapiOutput &output) noexcept;

	void FindExclusiveFormatSupported(AudioFormat &audio_format);
	void FindSharedFormatSupported(AudioFormat &audio_format);
	void EnumerateDevices();
	void GetDevice(unsigned int index);
	unsigned int SearchDevice(std::string_view name);
	void GetDefaultDevice();
};

WasapiOutput &wasapi_output_downcast(AudioOutput &output) noexcept {
	return static_cast<WasapiOutput &>(output);
}

bool wasapi_is_exclusive(WasapiOutput &output) noexcept { return output.is_exclusive; }

IMMDevice *wasapi_output_get_device(WasapiOutput &output) noexcept {
	return output.device.get();
}

IAudioClient *wasapi_output_get_client(WasapiOutput &output) noexcept {
	return output.client.get();
}

void WasapiOutputThread::Work() noexcept {
	FormatDebug(wasapi_output_domain, "Working thread started");
	try {
		com.emplace();
	} catch (...) {
		std::unique_lock<Mutex> lock(error.mutex);
		error.error_ptr = std::current_exception();
		error.cond.wait(lock);
		assert(error.error_ptr == nullptr);
		return;
	}
	while (true) {
		try {
			event->Wait(INFINITE);

			Status current_state = status.load();
			if (current_state == Status::FINISH) {
				FormatDebug(wasapi_output_domain,
					    "Working thread stopped");
				return;
			}

			AtScopeExit(&) { write.cond.notify_all(); };

			HRESULT result;
			UINT32 data_in_frames;
			result = client->GetCurrentPadding(&data_in_frames);
			if (FAILED(result)) {
				throw FormatHResultError(result,
							 "Failed to get current padding");
			}

			UINT32 write_in_frames = buffer_size_in_frames;
			if (!is_exclusive) {
				if (data_in_frames >= buffer_size_in_frames) {
					continue;
				}
				write_in_frames -= data_in_frames;
			} else if (data_in_frames >= buffer_size_in_frames * 2) {
				continue;
			}

			BYTE *data;

			result = render_client->GetBuffer(write_in_frames, &data);
			if (FAILED(result)) {
				throw FormatHResultError(result, "Failed to get buffer");
			}

			AtScopeExit(&) {
				render_client->ReleaseBuffer(write_in_frames, 0);
			};

			const UINT32 write_size = write_in_frames * frame_size;
			UINT32 new_data_size = 0;
			if (current_state == Status::PLAY) {
				new_data_size = spsc_buffer.pop(data, write_size);
			} else {
				FormatDebug(wasapi_output_domain,
					    "Working thread paused");
			}
			std::fill_n(data + new_data_size, write_size - new_data_size, 0);
		} catch (...) {
			std::unique_lock<Mutex> lock(error.mutex);
			error.error_ptr = std::current_exception();
			error.cond.wait(lock);
			assert(error.error_ptr == nullptr);
		}
	}
}

AudioOutput *WasapiOutput::Create(EventLoop &, const ConfigBlock &block) {
	return new WasapiOutput(block);
}

WasapiOutput::WasapiOutput(const ConfigBlock &block)
: AudioOutput(FLAG_ENABLE_DISABLE | FLAG_PAUSE),
  is_exclusive(block.GetBlockValue("exclusive", false)),
  enumerate_devices(block.GetBlockValue("enumerate", false)),
  device_config(block.GetBlockValue("device", "")) {}

void WasapiOutput::Enable() {
	com.emplace();
	event = std::make_shared<WinEvent>();
	enumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
				    CLSCTX_INPROC_SERVER);

	device_desc.clear();
	device.reset();

	if (enumerate_devices && SafeTry([this]() { EnumerateDevices(); })) {
		for (const auto &desc : device_desc) {
			FormatNotice(wasapi_output_domain, "Device \"%u\" \"%s\"",
				     desc.first, desc.second.c_str());
		}
	}

	unsigned int id = kErrorId;
	if (!device_config.empty()) {
		if (!SafeSilenceTry([this, &id]() { id = std::stoul(device_config); })) {
			id = SearchDevice(device_config);
		}
	}

	if (id != kErrorId) {
		SafeTry([this, id]() { GetDevice(id); });
	}

	if (!device) {
		GetDefaultDevice();
	}

	device_desc.clear();
}

void WasapiOutput::Disable() noexcept {
	if (thread) {
		try {
			thread->Finish();
			thread->Join();
		} catch (std::exception &err) {
			FormatError(wasapi_output_domain, "exception while disabling: %s",
				    err.what());
		}
		thread.reset();
		spsc_buffer.reset();
		client.reset();
	}
	device.reset();
	enumerator.reset();
	com.reset();
	event.reset();
}

void WasapiOutput::Open(AudioFormat &audio_format) {
	if (audio_format.channels == 0) {
		throw FormatInvalidArgument("channels should > 0");
	}

	client.reset();

	HRESULT result;
	result = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
				  client.AddressCast());
	if (FAILED(result)) {
		throw FormatHResultError(result, "Unable to activate audio client");
	}

	if (audio_format.format == SampleFormat::S24_P32) {
		audio_format.format = SampleFormat::S32;
	}
	if (audio_format.channels > 8) {
		audio_format.channels = 8;
	}

	if (Exclusive()) {
		FindExclusiveFormatSupported(audio_format);
	} else {
		FindSharedFormatSupported(audio_format);
	}

	using s = std::chrono::seconds;
	using ms = std::chrono::milliseconds;
	using ns = std::chrono::nanoseconds;
	using hundred_ns = std::chrono::duration<uint64_t, std::ratio<1, 10000000>>;

	// The unit in REFERENCE_TIME is hundred nanoseconds
	REFERENCE_TIME device_period;
	result = client->GetDevicePeriod(&device_period, nullptr);
	if (FAILED(result)) {
		throw FormatHResultError(result, "Unable to get device period");
	}
	FormatDebug(wasapi_output_domain, "Device period: %I64u ns",
		    size_t(ns(hundred_ns(device_period)).count()));

	REFERENCE_TIME buffer_duration = device_period;
	if (!Exclusive()) {
		const REFERENCE_TIME align = hundred_ns(ms(50)).count();
		buffer_duration = (align / device_period) * device_period;
	}
	FormatDebug(wasapi_output_domain, "Buffer duration: %I64u ns",
		    size_t(ns(hundred_ns(buffer_duration)).count()));

	if (Exclusive()) {
		result = client->Initialize(
			AUDCLNT_SHAREMODE_EXCLUSIVE,
			AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			buffer_duration, buffer_duration,
			reinterpret_cast<WAVEFORMATEX *>(&device_format), nullptr);
		if (result == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
			// https://docs.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-initialize
			UINT32 buffer_size_in_frames = 0;
			result = client->GetBufferSize(&buffer_size_in_frames);
			if (FAILED(result)) {
				throw FormatHResultError(
					result, "Unable to get audio client buffer size");
			}
			buffer_duration = std::ceil(
				double(buffer_size_in_frames * hundred_ns(s(1)).count()) /
				SampleRate());
			FormatDebug(wasapi_output_domain,
				    "Aligned buffer duration: %I64u ns",
				    size_t(ns(hundred_ns(buffer_duration)).count()));
			client.reset();
			result = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
						  nullptr, client.AddressCast());
			if (FAILED(result)) {
				throw FormatHResultError(
					result, "Unable to activate audio client");
			}
			result = client->Initialize(
				AUDCLNT_SHAREMODE_EXCLUSIVE,
				AUDCLNT_STREAMFLAGS_NOPERSIST |
					AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
				buffer_duration, buffer_duration,
				reinterpret_cast<WAVEFORMATEX *>(&device_format),
				nullptr);
		}
	} else {
		result = client->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			buffer_duration, 0,
			reinterpret_cast<WAVEFORMATEX *>(&device_format), nullptr);
	}

	if (FAILED(result)) {
		throw FormatHResultError(result, "Unable to initialize audio client");
	}

	ComPtr<IAudioRenderClient> render_client;
	result = client->GetService(IID_PPV_ARGS(render_client.Address()));
	if (FAILED(result)) {
		throw FormatHResultError(result, "Unable to get new render client");
	}

	result = client->SetEventHandle(event->handle());
	if (FAILED(result)) {
		throw FormatHResultError(result, "Unable to set event handler");
	}

	UINT32 buffer_size_in_frames;
	result = client->GetBufferSize(&buffer_size_in_frames);
	if (FAILED(result)) {
		throw FormatHResultError(result,
					 "Unable to get audio client buffer size");
	}

	watermark = buffer_size_in_frames * 3 * FrameSize();
	spsc_buffer = std::make_unique<boost::lockfree::spsc_queue<BYTE>>(
		buffer_size_in_frames * 4 * FrameSize());
	thread = std::make_unique<WasapiOutputThread>(
		event, client, std::move(render_client), FrameSize(),
		buffer_size_in_frames, is_exclusive, *spsc_buffer);
	thread->Start();
}

void WasapiOutput::Close() noexcept {
	assert(client && thread);
	Pause();
	thread->Finish();
	thread->Join();
	thread.reset();
	spsc_buffer.reset();
	client.reset();
}

std::chrono::steady_clock::duration WasapiOutput::Delay() const noexcept {
	if (!client || !is_started) {
		return std::chrono::steady_clock::duration::zero();
	}

	const size_t data_size = spsc_buffer->read_available();
	const size_t delay_size = std::max(data_size, watermark) - watermark;

	using s = std::chrono::seconds;
	using duration = std::chrono::steady_clock::duration;
	auto result = duration(s(delay_size)) / device_format.Format.nAvgBytesPerSec;
	return result;
}

size_t WasapiOutput::Play(const void *chunk, size_t size) {
	if (!client || !thread) {
		return 0;
	}

	do {
		const size_t consumed_size =
			spsc_buffer->push(static_cast<const BYTE *>(chunk), size);
		if (consumed_size == 0) {
			assert(is_started);
			thread->WaitWrite();
			continue;
		}

		if (!is_started) {
			is_started = true;

			thread->Play();

			HRESULT result;
			result = client->Start();
			if (FAILED(result)) {
				throw FormatHResultError(result,
							 "Failed to start client");
			}
		}

		thread->CheckException();

		return consumed_size;
	} while (true);
}

void WasapiOutput::Drain() {
	spsc_buffer->consume_all([](auto &&) {});
	thread->CheckException();
}

bool WasapiOutput::Pause() {
	if (!client || !thread) {
		return false;
	}
	if (!is_started) {
		return true;
	}

	HRESULT result;
	result = client->Stop();
	if (FAILED(result)) {
		throw FormatHResultError(result, "Failed to stop client");
	}

	is_started = false;
	thread->Pause();
	thread->CheckException();

	return true;
}

void WasapiOutput::FindExclusiveFormatSupported(AudioFormat &audio_format) {
	SetFormat(device_format, audio_format);

	do {
		HRESULT result;
		result = client->IsFormatSupported(
			AUDCLNT_SHAREMODE_EXCLUSIVE,
			reinterpret_cast<WAVEFORMATEX *>(&device_format), nullptr);

		switch (result) {
		case S_OK:
			return;
		case AUDCLNT_E_UNSUPPORTED_FORMAT:
			break;
		default:
			throw FormatHResultError(result, "IsFormatSupported failed");
		}

		// Trying PCM fallback.
		if (audio_format.format == SampleFormat::FLOAT) {
			audio_format.format = SampleFormat::S32;
			continue;
		}

		// Trying sample rate fallback.
		if (audio_format.sample_rate > 96000) {
			audio_format.sample_rate = 96000;
			continue;
		}

		if (audio_format.sample_rate > 88200) {
			audio_format.sample_rate = 88200;
			continue;
		}

		if (audio_format.sample_rate > 64000) {
			audio_format.sample_rate = 64000;
			continue;
		}

		if (audio_format.sample_rate > 48000) {
			audio_format.sample_rate = 48000;
			continue;
		}

		// Trying 2 channels fallback.
		if (audio_format.channels > 2) {
			audio_format.channels = 2;
			continue;
		}

		// Trying S16 fallback.
		if (audio_format.format == SampleFormat::S32) {
			audio_format.format = SampleFormat::S16;
			continue;
		}

		if (audio_format.sample_rate > 41100) {
			audio_format.sample_rate = 41100;
			continue;
		}

		throw FormatHResultError(result, "Format is not supported");
	} while (true);
}

void WasapiOutput::FindSharedFormatSupported(AudioFormat &audio_format) {
	HRESULT result;
	ComHeapPtr<WAVEFORMATEX> mixer_format;

	// In shared mode, different sample rate is always unsupported.
	result = client->GetMixFormat(mixer_format.Address());
	if (FAILED(result)) {
		throw FormatHResultError(result, "GetMixFormat failed");
	}
	audio_format.sample_rate = device_format.Format.nSamplesPerSec;

	SetFormat(device_format, audio_format);

	ComHeapPtr<WAVEFORMATEXTENSIBLE> closest_format;
	result = client->IsFormatSupported(
		AUDCLNT_SHAREMODE_SHARED,
		reinterpret_cast<WAVEFORMATEX *>(&device_format),
		closest_format.AddressCast<WAVEFORMATEX>());

	if (FAILED(result) && result != AUDCLNT_E_UNSUPPORTED_FORMAT) {
		throw FormatHResultError(result, "IsFormatSupported failed");
	}

	switch (result) {
	case S_OK:
		break;
	case AUDCLNT_E_UNSUPPORTED_FORMAT:
	default:
		// Trying channels fallback.
		audio_format.channels = mixer_format->nChannels;

		SetFormat(device_format, audio_format);

		result = client->IsFormatSupported(
			AUDCLNT_SHAREMODE_SHARED,
			reinterpret_cast<WAVEFORMATEX *>(&device_format),
			closest_format.AddressCast<WAVEFORMATEX>());
		if (FAILED(result)) {
			throw FormatHResultError(result, "Format is not supported");
		}
		break;
	case S_FALSE:
		if (closest_format->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
			device_format = *closest_format;
		} else {
			device_format.Samples.wValidBitsPerSample =
				device_format.Format.wBitsPerSample;
			device_format.Format = closest_format->Format;
			switch (std::exchange(device_format.Format.wFormatTag,
					      WAVE_FORMAT_EXTENSIBLE)) {
			case WAVE_FORMAT_PCM:
				device_format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
				break;
			case WAVE_FORMAT_IEEE_FLOAT:
				device_format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
				break;
			default:
				gcc_unreachable();
			}
		}
		break;
	}

	// Copy closest match back to audio_format.
	audio_format.channels = device_format.Format.nChannels;
	audio_format.sample_rate = device_format.Format.nSamplesPerSec;
	if (device_format.SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
		switch (device_format.Format.wBitsPerSample) {
		case 8:
			audio_format.format = SampleFormat::S8;
			break;
		case 16:
			audio_format.format = SampleFormat::S16;
			break;
		case 32:
			audio_format.format = SampleFormat::S32;
			break;
		}
	} else if (device_format.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
		audio_format.format = SampleFormat::FLOAT;
	}
}

void WasapiOutput::EnumerateDevices() {
	if (!device_desc.empty()) {
		return;
	}

	HRESULT result;

	ComPtr<IMMDeviceCollection> device_collection;
	result = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
						device_collection.Address());
	if (FAILED(result)) {
		throw FormatHResultError(result, "Unable to enumerate devices");
	}

	UINT count;
	result = device_collection->GetCount(&count);
	if (FAILED(result)) {
		throw FormatHResultError(result, "Collection->GetCount failed");
	}

	device_desc.reserve(count);
	for (UINT i = 0; i < count; ++i) {
		ComPtr<IMMDevice> enumerated_device;
		result = device_collection->Item(i, enumerated_device.Address());
		if (FAILED(result)) {
			throw FormatHResultError(result, "Collection->Item failed");
		}

		ComPtr<IPropertyStore> property_store;
		result = enumerated_device->OpenPropertyStore(STGM_READ,
							      property_store.Address());
		if (FAILED(result)) {
			throw FormatHResultError(result,
						 "Device->OpenPropertyStore failed");
		}

		PROPVARIANT var_name;
		PropVariantInit(&var_name);
		AtScopeExit(&) { PropVariantClear(&var_name); };

		result = property_store->GetValue(PKEY_Device_FriendlyName, &var_name);
		if (FAILED(result)) {
			throw FormatHResultError(result,
						 "PropertyStore->GetValue failed");
		}

		device_desc.emplace_back(
			i, WideCharToMultiByte(CP_UTF8,
					       std::wstring_view(var_name.pwszVal)));
	}
}

void WasapiOutput::GetDevice(unsigned int index) {
	HRESULT result;

	ComPtr<IMMDeviceCollection> device_collection;
	result = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
						device_collection.Address());
	if (FAILED(result)) {
		throw FormatHResultError(result, "Unable to enumerate devices");
	}

	result = device_collection->Item(index, device.Address());
	if (FAILED(result)) {
		throw FormatHResultError(result, "Collection->Item failed");
	}
}

unsigned int WasapiOutput::SearchDevice(std::string_view name) {
	if (!SafeTry([this]() { EnumerateDevices(); })) {
		return kErrorId;
	}
	auto iter =
		std::find_if(device_desc.cbegin(), device_desc.cend(),
			     [&name](const auto &desc) { return desc.second == name; });
	if (iter == device_desc.cend()) {
		FormatError(wasapi_output_domain, "Device %.*s not founded.",
			    int(name.size()), name.data());
		return kErrorId;
	}
	FormatInfo(wasapi_output_domain, "Select device \"%u\" \"%s\"", iter->first,
		   iter->second.c_str());
	return iter->first;
}

void WasapiOutput::GetDefaultDevice() {
	HRESULT result;
	result = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia,
						     device.Address());
	if (FAILED(result)) {
		throw FormatHResultError(result,
					 "Unable to get default device for multimedia");
	}
}

static bool wasapi_output_test_default_device() { return true; }

const struct AudioOutputPlugin wasapi_output_plugin = {
	"wasapi",
	wasapi_output_test_default_device,
	WasapiOutput::Create,
	&wasapi_mixer_plugin,
};
