// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "VisualizationClient.hxx"

#include "Log.hxx"
#include "event/Chrono.hxx"
#include "lib/fmt/ThreadIdFormatter.hxx"
#include "util/Domain.hxx"

#include <chrono>
#include <thread>
#include <variant>

const Domain d_vis_client("vis_client");

inline
typename std::chrono::microseconds::rep
NowTicks() {
	return duration_cast<std::chrono::microseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}


// Invoked when the client connects and the plugin is in the "closed" state.
Visualization::VisualizationClient::VisualizationClient(
	UniqueSocketDescriptor fd,
	EventLoop &event_loop,
	const SoundAnalysisParameters &params):
	BufferedSocket(fd.Release(), event_loop), // schedules a read
	sound_params(params),
	num_samp(params.GetNumSamples()),
	protocol_state(ProtocolState::Init),
	timer(event_loop, BIND_THIS_METHOD(OnTimer))
{ }

// Invoked when the client connects and the plugin is in the "opened" state.
Visualization::VisualizationClient::VisualizationClient(
	UniqueSocketDescriptor fd,
	EventLoop &event_loop,
	const SoundAnalysisParameters &params,
	const std::shared_ptr<SoundInfoCache> &pcache):
	BufferedSocket(fd.Release(), event_loop), // schedules a read
	sound_params(params),
	num_samp(params.GetNumSamples()),
	pcm_state(HavePcmData {
		pcache, Visualization::SoundAnalysis(params, pcache) }),
	protocol_state(ProtocolState::Init),
	timer(event_loop, BIND_THIS_METHOD(OnTimer))
{ }

void
Visualization::VisualizationClient::OnPluginOpened(
	const std::shared_ptr<SoundInfoCache> &pcache)
{
	FmtDebug(d_vis_client, "[{}] VisualizationClient::OnPluginOpened("
		 "this:{},tid:{},state:{})", NowTicks(), (size_t)this,
		 std::this_thread::get_id(), (int)protocol_state);

	pcm_state = HavePcmData {
		pcache, Visualization::SoundAnalysis(sound_params, pcache)
	};

	HandleFirstFrame();
}

void
Visualization::VisualizationClient::OnPluginClosed()
{
	FmtDebug(d_vis_client, "[{}] VisualizationClient::OnPluginClosed("
		 "this:{},tid:{},state:{})", NowTicks(), (size_t)this,
		 std::this_thread::get_id(), (int)protocol_state);

	if (IsClosed()) {
		Shutdown();
		return;
	}

	// Update `pcm_state`...
	pcm_state = std::monostate{};
	/* but no need to do anything else. We'll detect the fact that the
	   plugin is closed during subsequent state transitions & handle it
	   there. */
}

Visualization::VisualizationClient::~VisualizationClient() {
	FmtDebug(d_vis_client, "[{}] VisualizationClient::~VisualizationClient()"
		 "this:{},tid:{},state:{})", NowTicks(), (size_t)this,
		 std::this_thread::get_id(), (int)protocol_state);
	// This will be invoked on the main thread; the socket & underlying
	// `SocketEvent` will be torn-down on the I/O thread.
	timer.Cancel();
}

BufferedSocket::InputResult
Visualization::VisualizationClient::OnSocketInput(void *data,
						  size_t length) noexcept
{
	FmtDebug(d_vis_client, "[{}] VisualizationClient::OnSocketInput("
		 "this:{},tid:{},state:{},length:{})", NowTicks(),
		 (size_t)this, std::this_thread::get_id(),
			 (int)protocol_state, length);

	// We have data available to be read, and it's present in `data`...
	if (ProtocolState::Init != protocol_state) {
		Shutdown();
		return InputResult::CLOSED;
	}

	// attempt to parse it as a CLIHLO message...
	ClientHello clihlo;
	ParseResult parse_result = ParseClihlo(data, length, clihlo);
	if (ParseResult::NEED_MORE_DATA == parse_result) {
		return InputResult::MORE;
	} else if (ParseResult::ERROR == parse_result) {
		LogError(d_vis_client,
			 "Expected CLIHLO, received invalid message.");
		Shutdown();
		return InputResult::CLOSED;
	}

	FmtDebug(d_vis_client, "[{}] Got CLIHLO: {}fps, tau={}ms", NowTicks(),
			clihlo.requested_fps, clihlo.tau);

	if (0 != clihlo.major_version || 1 != clihlo.minor_version) {
		FmtWarning(d_vis_client, "Unexpected protocol version {}.{} "
			   "requested-- proceeding to serve 0.1.",
			   clihlo.major_version, clihlo.minor_version);
	}

	if (0 == clihlo.requested_fps) {
		LogError(d_vis_client,
			 "Client requested 0fps-- closing connection.");
		Shutdown();
		return InputResult::CLOSED;
	}

	// OK-- we have timings:
	timings = HaveClientInfo {
		std::chrono::milliseconds(clihlo.tau),
		std::chrono::milliseconds(int(1000. / clihlo.requested_fps))
	};

	// Seems legit-- compose our response...
	ConsumeInput(length);

	next_frame.clear();
	SerializeSrvhlo((std::byte)0, (std::byte)1, back_inserter(next_frame));

	FmtDebug(d_vis_client, "[{}] Composed a SRVHLO frame, cancelled read, "
		 "scheduled a write, and shifted to state {}.", NowTicks(),
			 (int)ProtocolState::SrvHlo);

	// shift state...
	protocol_state = ProtocolState::SrvHlo;
	// and schedule a write.
	event.CancelRead();
	event.ScheduleWrite();
	return InputResult::PAUSE;
}

void
Visualization::VisualizationClient::OnSocketError(
	std::exception_ptr ep) noexcept {
	LogError(ep);
	Shutdown();
}

void
Visualization::VisualizationClient::OnSocketClosed() noexcept {
	FmtInfo(d_vis_client, "[{}] VisualizationClient::OnSocketClosed("
		"this:{},tid:{})", NowTicks(), (size_t)this, std::this_thread::get_id());
	Shutdown();
}

void
Visualization::VisualizationClient::OnSocketReady(unsigned flags) noexcept
{
	FmtDebug(d_vis_client, "[{}] VisualizationClient::OnSocketReady("
		 "this:{},tid:{},state:{},flags:{})", NowTicks(), (size_t)this,
		 std::this_thread::get_id(), (int)protocol_state, flags);

	switch (protocol_state) {
	case ProtocolState::Init: {

		if (0 == (flags & SocketEvent::READ)) {
			FmtError(d_vis_client, "In state Init, got flags {} ("
				 "which do not contain READ/POLLIN); in this "
				 "state we expect a CLIHLO message.", flags);
			Shutdown();
			return;
		}

		if (flags & (SocketEvent::ERROR|SocketEvent::HANGUP)) {
			FmtError(d_vis_client, "In state Init, got flags {} "
				 "which contains ERROR and/or HANGUP, "
				 "shutting-down.", flags);
			Shutdown();
			return;
		}

		// Will invoke `OnSocketInput()`
		BufferedSocket::OnSocketReady(flags);
		break;
	}
	case ProtocolState::SrvHlo:
		HandleSrvHlo(flags);
		break;

	case ProtocolState::FrameReady:
		HandleFrameReady(flags);
		break;

	default:
		FmtError(d_vis_client, "VisualizationClient::OnSocketReady("
			 "tid: {}, flags: {}) invoked in state {}-- BAILING!",
			 std::this_thread::get_id(), flags, (int)protocol_state);
		Shutdown();
		return;
	}

}

/**
 * \brief Update our sound analysis
 *
 *
 * \return true if the analysis was successfully carried-out, false if it was
 * not
 *
 *
 * This method could fail to update the analysis for a few reasons:
 *
 * - the plugin could have been closed (in which case this implementation will
 *   shift to state ProtocolClosed) - the cache could not contain PCM data for
 *   the requested offset
 *
 * If this method returns true, the next FRAME is waiting in next_frame; the
 * caller is responsible for scheduling a write.
 *
 *
 */

bool
Visualization::VisualizationClient::ComposeSoundAnalysisFrame()
{
	using namespace std::chrono;

	FmtDebug(d_vis_client, "[{}] VisualizationClient::"
		 "ComposeSoundAnalysisFrame(this:{},tid:{},state:{})",
		 NowTicks(), (size_t)this, std::this_thread::get_id(), (int)protocol_state);

	if (!PluginIsOpen()) {
		protocol_state = ProtocolState::ProtocolClosed;
		return false;
	}

	auto now = system_clock::now();
	HavePcmData &pcm_data = std::get<HavePcmData>(pcm_state);
	if (!pcm_data.analysis.Update(now + timings->tau)) {
		return false;
	}

	/* At this point, the data we wish to transport on the wire is residing
	 * inside `pcm_data.analysis`. It needs to be transformed into it's
	 * interchange format (IEEE 754, big-endian, single precision), and
	 * moved into a buffer laid-out according to the protocol. That's one
	 * copy. I don't want to spend a lot of time optimizing this right now,
	 * but I'd like to avoid a second one-- we'll ask the `SoundAnalysis` to
	 * `transform()` the data with a unary operator & output iterator we
	 * provide. */

	SerializeSoundInfoFrame(pcm_data.analysis, back_inserter(next_frame));
	return true;
}

/**
 * \brief Handle the first frame-- if tau < 0 schedule the timer for -tau ms,
 * else write a frame immediately
 *
 *
 * If \c tau is less than zero, schedule a timer for -tau ms and shift state to
 * Waiting.
 *
 * If \c tau is non-negative, attempt to carry-out a sound analysis.
 *
 * If that succeeds, schedule a write of the newly-populated frame buffer,
 * schedule a write, and shift to state FrameReady.
 *
 * If the analysis failes, cancel any writes, schedule the timer for \a freq ms,
 * and shift to state Waiting.
 *
 *
 */

void
Visualization::VisualizationClient::HandleFirstFrame()
{
	auto tau = timings->tau;
	auto freq = timings->freq;
	if (tau < std::chrono::milliseconds::zero()) {
		FmtDebug(d_vis_client, "[{}] VisualizationClient::"
			 "HandleFirstFrame([this:{}]) scheduling a write for "
			 "{} ms from now & transitioning to state {}.",
			 NowTicks(), (size_t)this, -tau.count(),
			 (int)ProtocolState::Waiting);
		timer.Schedule(std::chrono::milliseconds(-tau));
		protocol_state = ProtocolState::Waiting;
	}
	else {
		if (ComposeSoundAnalysisFrame()) {
			FmtDebug(d_vis_client, "[{}] VisualizationClient::"
				 "HandleFirstFrame(this:{}) carried out sound "
				 "analysis, scheduled a write & is shifting to "
				 "state {}.", NowTicks(), (size_t)this,
				 (int)ProtocolState::FrameReady);
			event.ScheduleWrite();
			timer.Schedule(std::chrono::milliseconds(freq));
			protocol_state = ProtocolState::FrameReady;
		} else {
			FmtDebug(d_vis_client, "[{}] VisualizationClient::"
				 "OnPluginOpened(this:{}) failed to perform "
				 "sound analysis; cancelling any outstanding "
				 "writes, scheduling another attempt for {}ms "
				 "from now & shifting to state {}.",
				 NowTicks(), (size_t)this, freq.count(),
				 (int)ProtocolState::Waiting);
			event.CancelWrite();
			timer.Schedule(std::chrono::milliseconds(freq));
			protocol_state = ProtocolState::Waiting;
		}
	}
}

/**
 * \brief Handle socket events when in state FrameReady
 *
 *
 * \brief flags Flags indicating the nature of the socket event that occasiioned
 * this call
 *
 *
 * This function will handle errors, hangups, and writes. In the last case, it
 * will attempt to write the contents of next_frame. If successful, it will
 * shift state to Waiting.
 *
 *
 */

void
Visualization::VisualizationClient::HandleFrameReady(unsigned flags)
{
	if (0 == (flags & SocketEvent::WRITE)) {
		FmtError(d_vis_client, "In state FrameReady, got flags {} "
			 "(which do not contain WRITE/POLLOUT); in this state "
			 "we expect to be sending a sound analysis message.",
			 flags);
		Shutdown();
		return;
	}

	if (flags & (SocketEvent::ERROR|SocketEvent::HANGUP)) {
		FmtError(d_vis_client, "In state FrameReady, got flags {} which "
			 "contains ERROR and/or HANGUP, shutting-down.",
				 flags);
		Shutdown();
		return;
	}

	if (!WriteFrame()) {
		return;
	}

	// Timer should already be active
	protocol_state = ProtocolState::Waiting;
}

/**
 * \brief Handle socket events while in state SrvHlo
 *
 *
 * \brief flags Flags indicating the nature of the socket event that occasiioned
 * this call
 *
 *
 * This method expects the event to be a "write ready" and responds by writing
 * the contents of next_frame (presumably an SRVHLO message). If successful, and
 * the plugin is open, it will handle first frame chores. If the plugin is
 * closed, it will shift to state ProtocolClosed.
 *
 *
 */

void
Visualization::VisualizationClient::HandleSrvHlo(unsigned flags)
{
	if (0 == (flags & SocketEvent::WRITE)) {
		FmtError(d_vis_client, "In state SrvHlo, got flags {} (which "
			 "do not contain WRITE/POLLOUT); in this state we "
			 "expect to be sending an SRVHLO message.", flags);
		Shutdown();
		return;
	}

	if (flags & (SocketEvent::ERROR|SocketEvent::HANGUP)) {
		FmtError(d_vis_client, "In state SrvHlo, got flags {} which "
			 "contains ERROR and/or HANGUP, shutting-down.",
			 flags);
		Shutdown();
		return;
	}

	// The SRVHLO should be waiting for us in `next_frame`
	if (!WriteFrame()) {
		return;
	}

	if (PluginIsOpen()) {
		HandleFirstFrame();
	} else {
		FmtDebug(d_vis_client, "[{}] VisualizationClient::"
			 "HandleSrvHlo(): The visualization plugin is "
			 "closed; shifting to state {}.",
			 NowTicks(), (int)ProtocolState::ProtocolClosed);
		protocol_state = ProtocolState::ProtocolClosed;
		event.CancelWrite();
	}
}

void
Visualization::VisualizationClient::LogSocketWriteError(
	const socket_error_t &err) const noexcept
{
	if (IsSocketErrorSendWouldBlock(err)) {
		LogNotice(d_vis_client, "OnSocketReady invoked, but write "
			  "would block(!)");
		return;
	} else if (!IsSocketErrorClosed(err)) {
		SocketErrorMessage msg(err);
		FmtWarning(d_vis_client, "Failed to write to client: {}",
			   (const char *)msg);
	}
}

/* Timer callback-- invoked when it's time to compose the next sound analysis
 * frame. This will re-schedule the timer regardless of success or failure of
 * the sound analysis. */
void
Visualization::VisualizationClient::OnTimer() noexcept
{
	FmtDebug(d_vis_client, "[{}] VisualizationClient::OnTimer(this:{},"
		 "tid:{},state:{})", NowTicks(), (size_t)this, std::this_thread::get_id(),
		 (int)protocol_state);

	if (ComposeSoundAnalysisFrame()) {
		FmtDebug(d_vis_client, "VisualizationClient::OnTimer() "
			 "carried-out sound analysis, scheduled a write, "
			 "and shifted to state {}.",
			 (int)ProtocolState::FrameReady);
		event.ScheduleWrite();
		protocol_state = ProtocolState::FrameReady;
	} else {
		// Give up for now-- wait for the next timer event
		FmtDebug(d_vis_client, "VisualizationClient::OnTimer() "
			 "failed to carry-out sound analysis; cancelling "
			 "outstanding writes, shifting to state {}.",
			 (int)ProtocolState::Waiting);
		event.CancelWrite();
		protocol_state = ProtocolState::Waiting;
	}

	timer.Schedule(timings->freq);
}

void
Visualization::VisualizationClient::Shutdown() noexcept
{
	timer.Cancel();
	event.CancelRead();
	event.CancelWrite();
	BufferedSocket::Close();
	pcm_state = std::monostate{};
	protocol_state = ProtocolState::Done;
}

bool
Visualization::VisualizationClient::WriteFrame()
{
	ssize_t cb_written = GetSocket().Write({next_frame });
	if (0 > cb_written) {
		LogSocketWriteError(GetSocketError());
		Shutdown();
		return false;
	}

	ssize_t cb_expected = next_frame.end() - next_frame.begin();

	/* Handle the case of a partial write. The SRVHLO frame is always seven
	   octets in size. */
	if (cb_written < cb_expected) {
		FmtWarning(d_vis_client, "VisualizationClient::WriteFrame() "
			   "wrote {} bytes of message-- expected {}.",
			   cb_written, cb_expected);
		/* It's no problem, just remove the bytes that have been written
		 * from `next_frame`, schedule another write & bail. */
		next_frame.erase(next_frame.begin(),
				 next_frame.begin() + cb_written);
		event.ScheduleWrite();
		return false;
	}

	/* Finally, we should handle the case of `cb_written > 7`. Naturally,
	 * that "should" never happen, but I just can't leave the case
	 * uncovered. One could argue that an assertion would be justified, but
	 * I understand the maintainers to frown on assertions in production
	 * code, so: */
	if (cb_written > cb_expected) {
		FmtError(d_vis_client, "VisualizationClient::HandleSrvHlo() "
			 "wrote {} bytes, but {} were reported to have been "
			 "written-out. This should be investigated.",
			 cb_written, cb_expected);
	}

	FmtDebug(d_vis_client, "[{}] VisualizationClient::WriteFrame(tid:{},"
		 "state:{}) wrote {} bytes (of {}); cancelling any outstanding "
		 "writes & clearing the frame buffer.", NowTicks(), std::this_thread::get_id(),
		 (int)protocol_state, cb_written, cb_expected);

	event.CancelWrite();
	next_frame.clear();

	return true;
}
