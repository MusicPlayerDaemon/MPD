/*
 * Copyright 2015-2018 Cary Audio
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

#include "config.h"
#include "Context.hxx"
#include "CurlSocket.hxx"
#include "util/StringAPI.hxx"
#include "util/RuntimeError.hxx"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>
#include <ifaddrs.h>
#include <unistd.h>

namespace dms {

static int
system2(const char *cmdstring)
{
	pid_t pid;
	int status;

	if (cmdstring == nullptr) {
		return 1;
	}

	if ((pid = fork()) < 0) {
		status = -1;
	} else if (pid == 0) {
		execl("/system/bin/sh", "sh", "-c", cmdstring, nullptr);
		exit(127);
	} else {
		while(waitpid(pid, &status, 0) < 0) {
			if(errno != EINTR) {
				status = -1;
				break;
			}
		}
	}

	return status;
}

static int
system_with_back(const char* cmd, std::string &buf)
{
	int fd[2];
	int bak_fd;
	int new_fd;

	if(pipe(fd))   {
		printf("pipe error!\n");
		return -1;
	}

	bak_fd = dup(STDOUT_FILENO);
	new_fd = dup2(fd[1], STDOUT_FILENO);

	system2(cmd);
	char buffer[4096];
	size_t len = read(fd[0], buffer, sizeof(buffer)-1);
	if (len > 0) {
		buf.append(buffer, len);
	}
	dup2(bak_fd, new_fd);
	close(fd[0]);
	close(fd[1]);
	close(bak_fd);

	return len;
}

std::string
Context::getTidalOldRealUrl(std::string uri)
{
	if (uri.back() != '?') {
		uri.append("&");
	}
	uri.append("soundQuality").append("=").append(tidal.audioquality);
	if (!tidal.sessionId.empty()) {
		uri.append("&").append("sessionId").append("=").append(tidal.sessionId);
	}

	RealUrl real_url;
	CurlSocket::get(uri, real_url);

	return real_url.url;
}

std::string
Context::getTidalRealUrl(std::string uri)
{
	if (uri.find("/streamurl") != std::string::npos) {
		return getTidalOldRealUrl(uri);
	} else if (uri.find("/urlpostpaywall") != std::string::npos) {
		if (uri.back() != '?') {
			uri.append("&");
		}
		uri.append("assetpresentation=FULL&urlusagemode=STREAM");
		if (uri.find("audioquality") == std::string::npos) {
			assert(!tidal.audioquality.empty());
			uri.append("&audioquality").append("=").append(tidal.audioquality);
		}
		if (!tidal.sessionId.empty()) {
			uri.append("&").append("sessionId").append("=").append(tidal.sessionId);
		}

		RealUrl real_url;
		CurlSocket::get(uri, real_url);

		return real_url.url;
	}

	return std::string();
}

std::string
Context::acquireRealUrl(const std::string &uri)
try {
	if (StringFind(uri.c_str(), "api.tidalhifi.com") != nullptr ||
		StringFind(uri.c_str(), "api.tidal.com") != nullptr) {
		auto real_url = getTidalRealUrl(uri);
		return real_url;
	} else if (StringFind(uri.c_str(), "caryaudio.vtuner.com") != nullptr) {
		std::string cmd("/system/bin/curl_redirect \"");
		cmd.append(uri);
		cmd.append("\"");
		std::string new_uri;
		int len = system_with_back(cmd.c_str(), new_uri);
		while (!new_uri.empty() &&
			(new_uri.back() == '\r' || new_uri.back() == '\n')) {
			new_uri.pop_back();
		}
		if (len <= 0 || new_uri.empty()) {
			throw FormatRuntimeError("acquireRealUrl fail: %s", uri.c_str());
		}
		return new_uri;
	}

	return uri;
} catch (...) {
	return uri;
}

}
