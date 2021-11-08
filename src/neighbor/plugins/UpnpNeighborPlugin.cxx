/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "UpnpNeighborPlugin.hxx"
#include "lib/upnp/ClientInit.hxx"
#include "lib/upnp/Discovery.hxx"
#include "lib/upnp/ContentDirectoryService.hxx"
#include "neighbor/NeighborPlugin.hxx"
#include "neighbor/Explorer.hxx"
#include "neighbor/Listener.hxx"
#include "neighbor/Info.hxx"
#include "Log.hxx"

class UpnpNeighborExplorer final
	: public NeighborExplorer, UPnPDiscoveryListener {
	struct Server {
		std::string name, comment;

		bool alive;

		Server(std::string &&_name, std::string &&_comment)
			:name(std::move(_name)), comment(std::move(_comment)),
			 alive(true) {}
		Server(const Server &) = delete;
		Server &operator=(const Server &) = delete;

		[[gnu::pure]]
		bool operator==(const Server &other) const noexcept {
			return name == other.name;
		}

		[[nodiscard]] [[gnu::pure]]
		NeighborInfo Export() const noexcept {
			return { "smb://" + name + "/", comment };
		}
	};

	EventLoop &event_loop;

	UPnPDeviceDirectory *discovery;

public:
	UpnpNeighborExplorer(EventLoop &_event_loop,
			     NeighborListener &_listener)
		:NeighborExplorer(_listener), event_loop(_event_loop) {}

	/* virtual methods from class NeighborExplorer */
	void Open() override;
	void Close() noexcept override;
	[[nodiscard]] List GetList() const noexcept override;

private:
	/* virtual methods from class UPnPDiscoveryListener */
	void FoundUPnP(const ContentDirectoryService &service) override;
	void LostUPnP(const ContentDirectoryService &service) override;
};

void
UpnpNeighborExplorer::Open()
{
	auto handle = UpnpClientGlobalInit(nullptr);

	discovery = new UPnPDeviceDirectory(event_loop, handle, this);

	try {
		discovery->Start();
	} catch (...) {
		delete discovery;
		UpnpClientGlobalFinish();
		throw;
	}
}

void
UpnpNeighborExplorer::Close() noexcept
{
	delete discovery;
	UpnpClientGlobalFinish();
}

NeighborExplorer::List
UpnpNeighborExplorer::GetList() const noexcept
{
	std::vector<ContentDirectoryService> tmp;

	try {
		tmp = discovery->GetDirectories();
	} catch (...) {
		LogError(std::current_exception());
	}

	List result;
	for (const auto &i : tmp)
		result.emplace_front(i.GetURI(), i.getFriendlyName());
	return result;
}

void
UpnpNeighborExplorer::FoundUPnP(const ContentDirectoryService &service)
{
	const NeighborInfo n(service.GetURI(), service.getFriendlyName());
	listener.FoundNeighbor(n);
}

void
UpnpNeighborExplorer::LostUPnP(const ContentDirectoryService &service)
{
	const NeighborInfo n(service.GetURI(), service.getFriendlyName());
	listener.LostNeighbor(n);
}

static std::unique_ptr<NeighborExplorer>
upnp_neighbor_create(EventLoop &event_loop,
		     NeighborListener &listener,
		     [[maybe_unused]] const ConfigBlock &block)
{
	return std::make_unique<UpnpNeighborExplorer>(event_loop, listener);
}

const NeighborPlugin upnp_neighbor_plugin = {
	"upnp",
	upnp_neighbor_create,
};
