// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "config.h" // for USING_PUPNP

#include <upnp.h> // for UpnpClient_Handle

#include <utility> // for std::pair

#ifdef USING_PUPNP

#include <initializer_list>

class UpnpActionResponse {
	IXML_Document *document;

public:
	explicit UpnpActionResponse(IXML_Document *_document) noexcept
		:document(_document) {}

	~UpnpActionResponse() noexcept {
		ixmlDocument_free(document);
	}

	UpnpActionResponse(const UpnpActionResponse &) = delete;
	UpnpActionResponse &operator=(const UpnpActionResponse &) = delete;

	[[gnu::pure]]
	const char *GetValue(const char *name) const noexcept;
};

UpnpActionResponse
UpnpSendAction(UpnpClient_Handle handle, const char *url,
	       const char *action_name, const char *service_type,
	       std::initializer_list<std::pair<const char *, const char *>> args);

#else // USING_PUPNP

#include <string>
#include <vector>

class UpnpActionResponse {
	std::vector<std::pair<std::string, std::string>> data;

public:
	explicit UpnpActionResponse(std::vector<std::pair<std::string, std::string>> &&_data) noexcept
		:data(_data) {}

	UpnpActionResponse(const UpnpActionResponse &) = delete;
	UpnpActionResponse &operator=(const UpnpActionResponse &) = delete;

	[[gnu::pure]]
	const char *GetValue(std::string_view name) const noexcept {
		for (const auto &i : data)
			if (i.first == name)
				return i.second.c_str();

		return nullptr;
	}
};

UpnpActionResponse
UpnpSendAction(UpnpClient_Handle handle, const char *url,
	       const char *action_name, const char *service_type,
	       const std::vector<std::pair<std::string, std::string>> &args);

#endif // !USING_PUPNP
