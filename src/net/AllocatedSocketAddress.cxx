/*
 * Copyright (C) 2012-2015 Max Kellermann <max.kellermann@gmail.com>
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

#include "config.h"
#include "AllocatedSocketAddress.hxx"

#include <string.h>

#ifdef HAVE_UN
#include <sys/un.h>
#endif

AllocatedSocketAddress &
AllocatedSocketAddress::operator=(SocketAddress src)
{
	if (src.IsNull()) {
		Clear();
	} else {
		SetSize(src.GetSize());
		memcpy(address, src.GetAddress(), size);
	}

	return *this;
}

void
AllocatedSocketAddress::SetSize(size_type new_size) noexcept
{
	if (size == new_size)
		return;

	free(address);
	size = new_size;
	address = (struct sockaddr *)malloc(size);
}

#ifdef HAVE_UN

void
AllocatedSocketAddress::SetLocal(const char *path) noexcept
{
	const bool is_abstract = *path == '@';

	/* sun_path must be null-terminated unless it's an abstract
	   socket */
	const size_t path_length = strlen(path) + !is_abstract;

	struct sockaddr_un *sun;
	SetSize(sizeof(*sun) - sizeof(sun->sun_path) + path_length);
	sun = (struct sockaddr_un *)address;
	sun->sun_family = AF_UNIX;
	memcpy(sun->sun_path, path, path_length);

	if (is_abstract)
		sun->sun_path[0] = 0;
}

#endif
