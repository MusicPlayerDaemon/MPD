// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Action.hxx"
#include "Error.hxx"

#ifdef USING_PUPNP
#include "util/ScopeExit.hxx"

#include <upnptools.h>

static IXML_Document *
UpnpMakeAction(const char *action_name, const char *service_type,
	       std::initializer_list<std::pair<const char *, const char *>> args)
{
	IXML_Document *doc = UpnpMakeAction(action_name, service_type, 0, nullptr, nullptr);

	for (const auto &[name, value] : args)
		UpnpAddToAction(&doc, action_name, service_type, name, value);

	return doc;
}

const char *
UpnpActionResponse::GetValue(const char *name) const noexcept
{
	IXML_NodeList *nodes = ixmlDocument_getElementsByTagName(document, name);
	if (!nodes)
		return nullptr;

	AtScopeExit(nodes) { ixmlNodeList_free(nodes); };

	IXML_Node *first = ixmlNodeList_item(nodes, 0);
	if (!first)
		return nullptr;

	IXML_Node *dnode = ixmlNode_getFirstChild(first);
	if (!dnode)
		return nullptr;

	return ixmlNode_getNodeValue(dnode);
}

UpnpActionResponse
UpnpSendAction(UpnpClient_Handle handle, const char *url,
	       const char *action_name, const char *service_type,
	       std::initializer_list<std::pair<const char *, const char *>> args)
{
	IXML_Document *request = UpnpMakeAction(action_name, service_type, args);
	AtScopeExit(request) { ixmlDocument_free(request); };

	IXML_Document *response;
	int code = UpnpSendAction(handle, url, service_type, nullptr,
				  request, &response);
	if (code != UPNP_E_SUCCESS)
		throw Upnp::MakeError(code, "UpnpSendAction() failed");

	return UpnpActionResponse{response};
}

#else // USING_PUPNP

UpnpActionResponse
UpnpSendAction(UpnpClient_Handle handle, const char *url,
	       const char *action_name, const char *service_type,
	       const std::vector<std::pair<std::string, std::string>> &args)
{
	std::vector<std::pair<std::string, std::string>> params{args};
	std::vector<std::pair<std::string, std::string>> response;

	int errcode;
	std::string errdesc;
	int code = UpnpSendAction(handle, "", url, service_type, action_name,
				  params, response, &errcode, errdesc);
	if (code != UPNP_E_SUCCESS)
		throw Upnp::MakeError(code, "UpnpSendAction() failed");

	return UpnpActionResponse{std::move(response)};
}

#endif // !USING_PUPNP
