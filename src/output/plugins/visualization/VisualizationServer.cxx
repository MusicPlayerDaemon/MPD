// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "VisualizationServer.hxx"

#include "Log.hxx"
#include "lib/fmt/ThreadIdFormatter.hxx"
#include "config/Block.hxx"
#include "util/Domain.hxx"

using std::make_unique, std::move;

const Domain vis_server_domain("vis_server");

Visualization::VisualizationServer::VisualizationServer(
	EventLoop &event_loop,
	const char *bind_to_address,
	uint16_t port,
	size_t max_clients_in,
	const SoundAnalysisParameters &params_in)
: ServerSocket(event_loop),
  max_clients(max_clients_in),
  reaper(event_loop, BIND_THIS_METHOD(ReapClients)),
  sound_params(params_in)
{
	FmtInfo(vis_server_domain, "VisualizationServer::VisualizationServer("
		"{}:{}, {} clients maximum)", bind_to_address, port,
		max_clients);

	ServerSocketAddGeneric(*this, bind_to_address, port);
}

void
Visualization::VisualizationServer::ReapClients() noexcept
{
	FmtNotice(vis_server_domain, "VisualizationServer::ReapClients({}, "
			  "{} clients)", std::this_thread::get_id(), clients.size());

	for (auto p0 = clients.begin(), p1 = clients.end(); p0 != p1; ) {
		auto p = p0++;
		if (p->IsClosed()) {
			LogInfo(vis_server_domain, "Reaping closed client.");
			clients.erase(p);
		}
	}

	if (!clients.empty()) {
		LogInfo(vis_server_domain, "Scheduling another reaping in 3 "
			"seconds.");
		reaper.Schedule(std::chrono::seconds(3));
	}
}

void
Visualization::VisualizationServer::OnPluginOpened(
	const std::shared_ptr<SoundInfoCache> &pcache)
{
	state = HavePcmData{pcache };

	for (auto p0 = clients.begin(), p1 = clients.end(); p0 != p1; ) {
		auto p = p0++;
		if (! p->IsClosed()) {
			p->OnPluginOpened(pcache);
		}
	}
}

void
Visualization::VisualizationServer::OnPluginClosed()
{
	state = std::monostate {};

	for (auto p0 = clients.begin(), p1 = clients.end(); p0 != p1; ) {
		auto p = p0++;
		if (! p->IsClosed()) {
			p->OnPluginClosed();
		}
	}

}

void
Visualization::VisualizationServer::OnAccept(UniqueSocketDescriptor fd,
					     SocketAddress /*address*/,
					     int) noexcept
{
	FmtInfo(vis_server_domain, "VisualizationServer::OnAccept({})",
		std::this_thread::get_id());

	// Can we allow an additional client?
	if (max_clients && clients.size() >= max_clients) {
		FmtError(vis_server_domain, "Rejecting connection request; "
			 "the maximum number of clients ({}) has already been "
			 "reached.", max_clients);
	} else {
		if (state.index()) {
			auto have_pcm_data = get<HavePcmData>(state);
			clients.emplace_back(std::move(fd), GetEventLoop(), sound_params,
					     have_pcm_data.pcache);
		} else {
			clients.emplace_back(std::move(fd), GetEventLoop(),
					     sound_params);
		}
		reaper.Schedule(std::chrono::seconds(3));
	}
}
