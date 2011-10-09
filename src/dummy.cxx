/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

/*
 * Just a dummy C++ file that is linked to work around an automake
 * bug: automake uses CXXLD when at least one source is C++, but when
 * you link a static library with a C++ source, it uses CCLD.  This
 * causes linker problems (undefined reference to 'operator
 * delete(void*)'), because CCLD does not link with libstdc++.
 *
 * By linking with this empty C++ source, automake decides to use
 * CXXLD.
 *
 */
