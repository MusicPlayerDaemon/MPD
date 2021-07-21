/*
 * Copyright 2018 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef OPEN_HXX
#define OPEN_HXX

class FileDescriptor;
class UniqueFileDescriptor;

UniqueFileDescriptor
OpenReadOnly(const char *path, int flags=0);

UniqueFileDescriptor
OpenWriteOnly(const char *path, int flags=0);

#ifndef _WIN32

UniqueFileDescriptor
OpenDirectory(const char *name, int flags=0);

#endif

#ifdef __linux__

UniqueFileDescriptor
OpenPath(const char *path, int flags=0);

UniqueFileDescriptor
OpenPath(FileDescriptor directory, const char *name, int flags=0);

UniqueFileDescriptor
OpenReadOnly(FileDescriptor directory, const char *name, int flags=0);

UniqueFileDescriptor
OpenWriteOnly(FileDescriptor directory, const char *name, int flags=0);

UniqueFileDescriptor
OpenDirectory(FileDescriptor directory, const char *name, int flags=0);

#endif

#endif
