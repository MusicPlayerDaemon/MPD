// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Discovery.hxx"
#include "ContentDirectoryService.hxx"
#include "Device.hxx"
#include "Log.hxx"
#include "Error.hxx"
#include "lib/curl/Global.hxx"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Request.hxx"
#include "event/Call.hxx"
#include "event/InjectEvent.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/ScopeExit.hxx"
#include "util/SpanCast.hxx"

#include <upnptools.h>

#include <stdlib.h>

class UPnPDeviceDirectory::ContentDirectoryDescriptor {
public:
	std::string id;

	UPnPDevice device;

	/**
	 * The time stamp when this device expires.
	 */
	std::chrono::steady_clock::time_point expires;

	ContentDirectoryDescriptor(std::string &&_id,
				   std::chrono::steady_clock::time_point last,
				   std::chrono::steady_clock::duration exp) noexcept
		:id(std::move(_id)),
		 expires(last + exp + std::chrono::seconds(20)) {}

	void Parse(std::string_view url, std::string_view description) {
		device.Parse(url, description);
	}
};

class UPnPDeviceDirectory::Downloader final
	: public IntrusiveListHook<>, CurlResponseHandler
{
	InjectEvent defer_start_event;

	UPnPDeviceDirectory &parent;

	std::string id;
	const std::string url;
	const std::chrono::steady_clock::duration expires;

	CurlRequest request;

	std::string data;

public:
	Downloader(UPnPDeviceDirectory &_parent,
		   const UpnpDiscovery &disco);

	void Start() noexcept {
		defer_start_event.Schedule();
	}

	void Destroy() noexcept;

private:
	void OnDeferredStart() noexcept {
		try {
			request.Start();
		} catch (...) {
			OnError(std::current_exception());
		}
	}

	/* virtual methods from CurlResponseHandler */
	void OnHeaders(unsigned status, Curl::Headers &&headers) override;
	void OnData(std::span<const std::byte> data) override;
	void OnEnd() override;
	void OnError(std::exception_ptr e) noexcept override;
};

UPnPDeviceDirectory::Downloader::Downloader(UPnPDeviceDirectory &_parent,
					    const UpnpDiscovery &disco)
	:defer_start_event(_parent.GetEventLoop(),
			   BIND_THIS_METHOD(OnDeferredStart)),
	 parent(_parent),
	 id(UpnpDiscovery_get_DeviceID_cstr(&disco)),
	 url(UpnpDiscovery_get_Location_cstr(&disco)),
	 expires(std::chrono::seconds(UpnpDiscovery_get_Expires(&disco))),
	 request(*parent.curl, url.c_str(), *this)
{
	const std::scoped_lock<Mutex> protect(parent.mutex);
	parent.downloaders.push_back(*this);
}

void
UPnPDeviceDirectory::Downloader::Destroy() noexcept
{
	const std::scoped_lock<Mutex> protect(parent.mutex);
	unlink();
	delete this;
}

void
UPnPDeviceDirectory::Downloader::OnHeaders(unsigned status,
					   Curl::Headers &&)
{
	if (status != 200) {
		Destroy();
		return;
	}
}

void
UPnPDeviceDirectory::Downloader::OnData(std::span<const std::byte> src)
{
	data.append(ToStringView(src));
}

void
UPnPDeviceDirectory::Downloader::OnEnd()
{
	AtScopeExit(this) { Destroy(); };

	ContentDirectoryDescriptor d(std::move(id),
				     std::chrono::steady_clock::now(),
				     expires);

	try {
		d.Parse(url, data);
	} catch (...) {
		LogError(std::current_exception());
	}

	parent.LockAdd(std::move(d));
}

void
UPnPDeviceDirectory::Downloader::OnError(std::exception_ptr e) noexcept
{
	LogError(e);
	Destroy();
}

// The service type string we are looking for.
static constexpr char ContentDirectorySType[] = "urn:schemas-upnp-org:service:ContentDirectory:1";

// We don't include a version in comparisons, as we are satisfied with
// version 1
[[gnu::pure]]
static bool
isCDService(std::string_view st) noexcept
{
	std::string_view prefix = ContentDirectorySType;
	prefix.remove_suffix(2);

	return st.starts_with(prefix);
}

// The type of device we're asking for in search
static constexpr char MediaServerDType[] = "urn:schemas-upnp-org:device:MediaServer:1";

[[gnu::pure]]
static bool
isMSDevice(std::string_view st) noexcept
{
	std::string_view prefix = MediaServerDType;
	prefix.remove_suffix(2);

	return st.starts_with(prefix);
}

static void
AnnounceFoundUPnP(UPnPDiscoveryListener &listener, const UPnPDevice &device)
{
	for (const auto &service : device.services)
		if (isCDService(service.serviceType))
			listener.FoundUPnP(ContentDirectoryService(device,
								   service));
}

static void
AnnounceLostUPnP(UPnPDiscoveryListener &listener, const UPnPDevice &device)
{
	for (const auto &service : device.services)
		if (isCDService(service.serviceType))
			listener.LostUPnP(ContentDirectoryService(device,
								  service));
}

inline void
UPnPDeviceDirectory::LockAdd(ContentDirectoryDescriptor &&d)
{
	const std::scoped_lock<Mutex> protect(mutex);

	for (auto &i : directories) {
		if (i.id == d.id) {
			i = std::move(d);
			return;
		}
	}

	directories.emplace_back(std::move(d));

	if (listener != nullptr)
		AnnounceFoundUPnP(*listener, directories.back().device);
}

inline void
UPnPDeviceDirectory::LockRemove(const std::string &id)
{
	const std::scoped_lock<Mutex> protect(mutex);

	for (auto i = directories.begin(), end = directories.end();
	     i != end; ++i) {
		if (i->id == id) {
			if (listener != nullptr)
				AnnounceLostUPnP(*listener, i->device);

			directories.erase(i);
			break;
		}
	}
}

inline int
UPnPDeviceDirectory::OnAlive(const UpnpDiscovery *disco) noexcept
{
	if (isMSDevice(UpnpDiscovery_get_DeviceType_cstr(disco)) ||
	    isCDService(UpnpDiscovery_get_ServiceType_cstr(disco))) {
		try {
			auto *downloader = new Downloader(*this, *disco);
			downloader->Start();
		} catch (...) {
			LogError(std::current_exception());
			return UPNP_E_SUCCESS;
		}
	}

	return UPNP_E_SUCCESS;
}

inline int
UPnPDeviceDirectory::OnByeBye(const UpnpDiscovery *disco) noexcept
{
	if (isMSDevice(UpnpDiscovery_get_DeviceType_cstr(disco)) ||
	    isCDService(UpnpDiscovery_get_ServiceType_cstr(disco))) {
		// Device signals it is going off.
		LockRemove(UpnpDiscovery_get_DeviceID_cstr(disco));
	}

	return UPNP_E_SUCCESS;
}

// This gets called for all libupnp asynchronous events, in a libupnp
// thread context.
// Example: ContentDirectories appearing and disappearing from the network
// We queue a task for our worker thread(s)
int
UPnPDeviceDirectory::Invoke(Upnp_EventType et, const void *evp) noexcept
{
	switch (et) {
	case UPNP_DISCOVERY_SEARCH_RESULT:
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		{
			auto *disco = (const UpnpDiscovery *)evp;
			return OnAlive(disco);
		}

	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
		{
			auto *disco = (const UpnpDiscovery *)evp;
			return OnByeBye(disco);
		}

	default:
		// Ignore other events for now
		break;
	}

	return UPNP_E_SUCCESS;
}

void
UPnPDeviceDirectory::ExpireDevices()
{
	const auto now = std::chrono::steady_clock::now();
	bool didsomething = false;

	directories.remove_if([now, &didsomething](const ContentDirectoryDescriptor &d){
			bool expired = now > d.expires;
			if (expired)
				didsomething = true;
			return expired;
		});

	if (didsomething)
		Search();
}

UPnPDeviceDirectory::UPnPDeviceDirectory(EventLoop &event_loop,
					 UpnpClient_Handle _handle,
					 UPnPDiscoveryListener *_listener)
	:curl(event_loop), handle(_handle),
	 listener(_listener)
{
}

UPnPDeviceDirectory::~UPnPDeviceDirectory() noexcept
{
	BlockingCall(GetEventLoop(), [this]() {
		const std::scoped_lock<Mutex> protect(mutex);
		downloaders.clear_and_dispose(DeleteDisposer());
	});
}

inline EventLoop &
UPnPDeviceDirectory::GetEventLoop() const noexcept
{
	return curl->GetEventLoop();
}

void
UPnPDeviceDirectory::Start()
{
	Search();
}

void
UPnPDeviceDirectory::Search()
{
	const auto now = std::chrono::steady_clock::now();
	if (now - last_search < std::chrono::seconds(10))
		return;
	last_search = now;

	// We search both for device and service just in case.
	int code = UpnpSearchAsync(handle, search_timeout,
				   ContentDirectorySType, GetUpnpCookie());
	if (code != UPNP_E_SUCCESS)
		throw Upnp::MakeError(code, "UpnpSearchAsync() failed");

	code = UpnpSearchAsync(handle, search_timeout,
			       MediaServerDType, GetUpnpCookie());
	if (code != UPNP_E_SUCCESS)
		throw Upnp::MakeError(code, "UpnpSearchAsync() failed");
}

std::vector<ContentDirectoryService>
UPnPDeviceDirectory::GetDirectories()
{
	const std::scoped_lock<Mutex> protect(mutex);

	ExpireDevices();

	std::vector<ContentDirectoryService> out;
	for (const auto &descriptor : directories) {
		for (const auto &service : descriptor.device.services) {
			if (isCDService(service.serviceType)) {
				out.emplace_back(descriptor.device, service);
			}
		}
	}

	return out;
}

ContentDirectoryService
UPnPDeviceDirectory::GetServer(std::string_view friendly_name)
{
	const std::scoped_lock<Mutex> protect(mutex);

	ExpireDevices();

	for (const auto &i : directories) {
		const auto &device = i.device;

		if (device.friendlyName != friendly_name)
			continue;

		for (const auto &service : device.services)
			if (isCDService(service.serviceType))
				return {device, service};
	}

	throw std::runtime_error("Server not found");
}
