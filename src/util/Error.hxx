/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#ifndef MPD_ERROR_HXX
#define MPD_ERROR_HXX

#include "check.h"
#include "Compiler.h"

#include <string>

#include <assert.h>

class Domain;

extern const Domain errno_domain;

#ifdef WIN32
/* fuck WIN32! */
#include <windows.h>
#define IgnoreError MPDIgnoreError
#undef GetMessage

/**
 * Domain for GetLastError().
 */
extern const Domain win32_domain;
#endif

/**
 * This class contains information about a runtime error.
 */
class Error {
	const Domain *domain;
	int code;
	std::string message;

public:
	Error():domain(nullptr), code(0) {}

	Error(const Domain &_domain, int _code, const char *_message)
		:domain(&_domain), code(_code), message(_message) {}

	Error(const Domain &_domain, const char *_message)
		:domain(&_domain), code(0), message(_message) {}

	Error(Error &&other)
		:domain(other.domain), code(other.code),
		 message(std::move(other.message)) {}

	~Error();

	Error(const Error &) = delete;
	Error &operator=(const Error &) = delete;

	Error &operator=(Error &&other) {
		domain = other.domain;
		code = other.code;
		std::swap(message, other.message);
		return *this;
	}

	bool IsDefined() const {
		return domain != nullptr;
	}

	void Clear() {
		domain = nullptr;
	}

	const Domain &GetDomain() const {
		assert(IsDefined());

		return *domain;
	}

	bool IsDomain(const Domain &other) const {
		return domain == &other;
	}

	int GetCode() const {
		assert(IsDefined());

		return code;
	}

	const char *GetMessage() const {
		assert(IsDefined());

		return message.c_str();
	}

	void Set(const Error &other) {
		assert(!IsDefined());
		assert(other.IsDefined());

		domain = other.domain;
		code = other.code;
		message = other.message;
	}

	void Set(const Domain &_domain, int _code, const char *_message);

	void Set(const Domain &_domain, const char *_message) {
		Set(_domain, 0, _message);
	}

private:
	void Format2(const Domain &_domain, int _code, const char *fmt, ...);

public:
	template<typename... Args>
	void Format(const Domain &_domain, int _code,
		    const char *fmt, Args&&... args) {
		Format2(_domain, _code, fmt, std::forward<Args>(args)...);
	}

	template<typename... Args>
	void Format(const Domain &_domain, const char *fmt, Args&&... args) {
		Format2(_domain, 0, fmt, std::forward<Args>(args)...);
	}

	void AddPrefix(const char *prefix) {
		message.insert(0, prefix);
	}

	void FormatPrefix(const char *fmt, ...);

	void SetErrno(int e);
	void SetErrno();
	void SetErrno(int e, const char *prefix);
	void SetErrno(const char *prefix);
	void FormatErrno(const char *prefix, ...);
	void FormatErrno(int e, const char *prefix, ...);

#ifdef WIN32
	void SetLastError(const char *prefix);
#endif
};

/**
 * Pass a temporary instance of this class to ignore errors.
 */
class IgnoreError final {
	Error error;

public:
	operator Error &() {
		assert(!error.IsDefined());

		return error;
	}
};

#endif
