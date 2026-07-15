// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Control.hxx"
#include "Filtered.hxx"
#include "Client.hxx"
#include "Domain.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "mixer/Mixer.hxx"
#include "config/Block.hxx"
#include "Log.hxx"

#include <cassert>

/** after a failure, wait this duration before
    automatically reopening the device */
static constexpr PeriodClock::Duration REOPEN_AFTER = std::chrono::seconds(10);

AudioOutputControl::AudioOutputControl(std::unique_ptr<FilteredAudioOutput> _output,
				       const ConfigBlock &block)
	:output(std::move(_output)),
	 tags(block.GetBlockValue("tags", true)),
	 always_on(block.GetBlockValue("always_on", false)),
	 always_off(block.GetBlockValue("always_off", false)),
	 enabled(block.GetBlockValue("enabled", true))
{
	assert(output->mixer_listener == nullptr);
	output->mixer_listener = this;
}

AudioOutputControl::~AudioOutputControl() noexcept
{
	StopThread();
}

const char *
AudioOutputControl::GetName() const noexcept
{
	return output->GetName();
}

const char *
AudioOutputControl::GetPluginName() const noexcept
{
	return output->GetPluginName();
}

const char *
AudioOutputControl::GetLogName() const noexcept
{
	return output->GetLogName();
}

Mixer *
AudioOutputControl::GetMixer() const noexcept
{
	return output->mixer;
}

std::map<std::string, std::string, std::less<>>
AudioOutputControl::GetAttributes() const noexcept
{
	return output->GetAttributes();
}

void
AudioOutputControl::SetAttribute(std::string &&attribute_name,
				 std::string &&value)
{
	output->SetAttribute(std::move(attribute_name), std::move(value));
}

bool
AudioOutputControl::LockSetEnabled(bool new_value) noexcept
{
	const std::lock_guard protect{mutex};

	if (new_value == enabled)
		return false;

	enabled = new_value;
	return true;
}

bool
AudioOutputControl::LockToggleEnabled() noexcept
{
	const std::lock_guard protect{mutex};
	return enabled = !enabled;
}

void
AudioOutputControl::WaitForCommand(std::unique_lock<Mutex> &lock) noexcept
{
	client_cond.wait(lock, [this]{ return IsCommandFinished(); });
}

void
AudioOutputControl::CommandAsync(Command cmd) noexcept
{
	assert(IsCommandFinished());

	command = cmd;
	wake_cond.notify_one();
}

void
AudioOutputControl::CommandWait(std::unique_lock<Mutex> &lock,
				Command cmd) noexcept
{
	CommandAsync(cmd);
	WaitForCommand(lock);
}

void
AudioOutputControl::LockCommandWait(Command cmd) noexcept
{
	std::unique_lock lock{mutex};
	CommandWait(lock, cmd);
}

void
AudioOutputControl::LockDisable() noexcept
{
	std::unique_lock lock{mutex};

	if (really_enabled && output->SupportsEnableDisable())
		CommandWait(lock, Command::DISABLE);

	enabled = really_enabled = false;
}

void
AudioOutputControl::EnableAsync()
{
	if (always_off)
		return;

	if (!thread.IsDefined()) {
		if (!output->SupportsEnableDisable()) {
			/* don't bother to start the thread now if the
			   device doesn't even have a enable() method;
			   just assign the variable and we're done */
			really_enabled = true;
			return;
		}

		StartThread();
	}

	CommandAsync(Command::ENABLE);
}

void
AudioOutputControl::DisableAsync() noexcept
{
	if (!thread.IsDefined()) {
		if (!output->SupportsEnableDisable())
			really_enabled = false;
		else
			/* if there's no thread yet, the device cannot
			   be enabled */
			assert(!really_enabled);

		return;
	}

	CommandAsync(Command::DISABLE);
}

void
AudioOutputControl::EnableDisableAsync()
{
	if (enabled == really_enabled)
		return;

	if (enabled)
		EnableAsync();
	else
		DisableAsync();
}

inline bool
AudioOutputControl::Open(std::unique_lock<Mutex> &&lock,
			 const AudioFormat audio_format,
			 const MusicPipe &mp) noexcept
{
	assert(allow_play);
	assert(audio_format.IsValid());

	fail_timer.Reset();

	if (open && audio_format == request.audio_format) {
		assert(request.pipe == &mp || (always_on && pause));

		if (!pause && !should_reopen)
			/* already open, already the right parameters
			   - nothing needs to be done */
			return true;
	}

	request.audio_format = audio_format;
	request.pipe = &mp;

	if (!thread.IsDefined()) {
		try {
			StartThread();
		} catch (...) {
			LogError(std::current_exception());
			return false;
		}
	}

	CommandWait(lock, Command::OPEN);
	const bool open2 = open;

	if (open2 && output->mixer != nullptr) {
		lock.unlock();

		try {
			output->mixer->LockOpen();
		} catch (...) {
			FmtError(output_domain,
				 "Failed to open mixer for {:?}: {}",
				 GetName(), std::current_exception());
		}
	}

	return open2;
}

void
AudioOutputControl::CloseWait(std::unique_lock<Mutex> &lock) noexcept
{
	assert(allow_play);

	if (output->mixer != nullptr)
		output->mixer->LockAutoClose();

	assert(!open || !fail_timer.IsDefined());

	if (open)
		CommandWait(lock, Command::CLOSE);
	else
		fail_timer.Reset();
}

bool
AudioOutputControl::LockUpdate(const AudioFormat audio_format,
			       const MusicPipe &mp,
			       bool force) noexcept
{
	std::unique_lock lock{mutex};

	if (enabled && really_enabled) {
		if (force || !fail_timer.IsDefined() ||
		    fail_timer.Check(REOPEN_AFTER)) {
			return Open(std::move(lock), audio_format, mp);
		}
	} else if (IsOpen())
		CloseWait(lock);

	return false;
}

bool
AudioOutputControl::IsChunkConsumed(const MusicChunk &chunk) const noexcept
{
	if (!open)
		return true;

	return source.IsChunkConsumed(chunk);
}

bool
AudioOutputControl::LockIsChunkConsumed(const MusicChunk &chunk) const noexcept
{
	const std::lock_guard protect{mutex};
	return IsChunkConsumed(chunk);
}

void
AudioOutputControl::LockPlay() noexcept
{
	const std::lock_guard protect{mutex};

	assert(allow_play);

	if (IsOpen() && !in_playback_loop && !woken_for_play) {
		woken_for_play = true;
		wake_cond.notify_one();
	}
}

void
AudioOutputControl::LockPauseAsync() noexcept
{
	if (output && output->mixer != nullptr && !output->SupportsPause())
		/* the device has no pause mode: close the mixer,
		   unless its "global" flag is set (checked by
		   Mixer::LockAutoClose()) */
		output->mixer->LockAutoClose();

	output->Interrupt();

	const std::lock_guard protect{mutex};

	assert(allow_play);
	if (IsOpen())
		CommandAsync(Command::PAUSE);
}

void
AudioOutputControl::LockDrainAsync() noexcept
{
	const std::lock_guard protect{mutex};

	assert(allow_play);
	if (IsOpen())
		CommandAsync(Command::DRAIN);
}

void
AudioOutputControl::LockCancelAsync() noexcept
{
	output->Interrupt();

	const std::lock_guard protect{mutex};

	if (IsOpen()) {
		allow_play = false;
		CommandAsync(Command::CANCEL);
	}
}

void
AudioOutputControl::LockAllowPlay() noexcept
{
	const std::lock_guard protect{mutex};

	allow_play = true;
	if (IsOpen())
		wake_cond.notify_one();
}

void
AudioOutputControl::LockRelease() noexcept
{
	output->Interrupt();

	if (output->mixer != nullptr &&
	    (!always_on || !output->SupportsPause()))
		/* the device has no pause mode: close the mixer,
		   unless its "global" flag is set (checked by
		   Mixer::LockAutoClose()) */
		output->mixer->LockAutoClose();

	std::unique_lock lock{mutex};

	assert(!open || !fail_timer.IsDefined());
	assert(allow_play);

	if (IsOpen())
		CommandWait(lock, Command::RELEASE);
	else
		fail_timer.Reset();
}

void
AudioOutputControl::LockCloseWait() noexcept
{
	assert(!open || !fail_timer.IsDefined());

	output->Interrupt();

	std::unique_lock lock{mutex};
	CloseWait(lock);
}

void
AudioOutputControl::BeginDestroy() noexcept
{
	if (thread.IsDefined()) {
		output->Interrupt();

		const std::lock_guard protect{mutex};
		if (!killed) {
			killed = true;
			CommandAsync(Command::KILL);
		}
	}
}

void
AudioOutputControl::StopThread() noexcept
{
	BeginDestroy();

	if (thread.IsDefined())
		thread.Join();

	assert(IsCommandFinished());
}

void
AudioOutputControl::OnMixerVolumeChanged(Mixer &_mixer, int volume) noexcept
{
	const std::lock_guard lock{mutex};

	if (mixer_listener != nullptr)
		mixer_listener->OnMixerVolumeChanged(_mixer, volume);
}

void
AudioOutputControl::OnMixerChanged() noexcept
{
	const std::lock_guard lock{mutex};

	if (mixer_listener != nullptr)
		mixer_listener->OnMixerChanged();
}
