/*
 * Copyright (C) 2013 Max Kellermann <max@duempel.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ERROR_HXX
#define ERROR_HXX

#include "check.h"
#include "Compiler.h"

#include <string>
#include <utility>

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

	gcc_printf(2,3)
	void FormatPrefix(const char *fmt, ...);

	void SetErrno(int e);
	void SetErrno();
	void SetErrno(int e, const char *prefix);
	void SetErrno(const char *prefix);

	gcc_printf(2,3)
	void FormatErrno(const char *prefix, ...);

	gcc_printf(3,4)
	void FormatErrno(int e, const char *prefix, ...);

#ifdef WIN32
	void SetLastError(DWORD _code, const char *prefix);
	void SetLastError(const char *prefix);

	gcc_printf(3,4)
	void FormatLastError(DWORD code, const char *fmt, ...);

	gcc_printf(2,3)
	void FormatLastError(const char *fmt, ...);
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
