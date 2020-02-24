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

#ifndef QOBUZ_REQUEST_HXX
#define QOBUZ_REQUEST_HXX

#include "check.h"
#include "QobuzClient.hxx"
#include "util/RuntimeError.hxx"
#include "lib/curl/Slist.hxx"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Request.hxx"

#include <sstream>

class QobuzHandler {
public:
	virtual ~QobuzHandler() {}
	virtual void OnQobuzSuccess() noexcept = 0;
	virtual void OnQobuzError(std::exception_ptr error) noexcept = 0;
};

template<class T>
class QobuzRequest final : CurlResponseHandler {
	CurlSlist request_headers;

	CurlRequest request;

	QobuzHandler &handler;

	std::stringstream ss;

	T &item;

public:
	QobuzRequest(QobuzClient &client,
			  T &_item,
			  const char *request_url,
			  QobuzHandler &_handler)
		: request(client.GetCurl(), request_url, *this)
		, handler(_handler)
		, item(_item) {
		request_headers.Append(("X-User-Auth-Token:"
					+ client.GetSession().user_auth_token).c_str());
		request.SetOption(CURLOPT_HTTPHEADER, request_headers.Get());
	}

	~QobuzRequest() noexcept {
		request.StopIndirect();
	}

	void Start() noexcept {
		request.StartIndirect();
	}

	void SetPost(const std::string &data=std::string()) {
		request.SetOption(CURLOPT_POST, 1L);
		if (!data.empty()) {
			request.SetOption(CURLOPT_POSTFIELDS, data.c_str());
		}
	}

private:
	/**
	 * Status line and headers have been received.
	 */
	void OnHeaders(unsigned status,
			       std::multimap<std::string, std::string> &&headers) override {
		if (status != 200)
			throw FormatRuntimeError("Qobuz error: %d", status);

		auto i = headers.find("content-type");
		if (i == headers.end() || i->second.find("/json") == i->second.npos)
			throw std::runtime_error("Not a JSON response from Qobuz");
	}

	/**
	 * Response body data has been received.
	 */
	void OnData(ConstBuffer<void> data) override {
		ss.write((const char*)data.data, data.size);
	}

	/**
	 * The response has ended.  The method is allowed delete the
	 * #CurlRequest here.
	 */
	void OnEnd() override {
		try {
			jaijson::Document doc;
			auto s = ss.str();
			if (doc.Parse(s.c_str()).HasParseError()) {
				throw std::runtime_error("parse json fail");
			}
			deserialize(doc, item);
			handler.OnQobuzSuccess();
		} catch (...) {
			handler.OnQobuzError(std::current_exception());
		}
	}

	/**
	 * An error has occurred.  The method is allowed delete the
	 * #CurlRequest here.
	 */
	void OnError(std::exception_ptr e, gcc_unused int code) noexcept override {
		handler.OnQobuzError(e);
	}
};

#endif
