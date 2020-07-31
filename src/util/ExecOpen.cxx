/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#include "ExecOpen.hxx"
#include "system/Error.hxx"
#include "util/ScopeExit.hxx"

#include <errno.h>
#include <spawn.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

FILE *exec_open(int *pid, const char *cmd, const char *const args[])
{

	int fd[2];
	if(pipe2(fd, O_CLOEXEC) < 0) {
		return nullptr;
	}

	AtScopeExit(&fd) {
		close(fd[1]);
	};

	posix_spawn_file_actions_t spawn_file;
	posix_spawn_file_actions_init(&spawn_file);
	posix_spawn_file_actions_adddup2(&spawn_file, fd[1], STDOUT_FILENO);

	errno = posix_spawnp(pid, "youtube-dl", &spawn_file, nullptr, const_cast<char**>(args), nullptr);
	if(errno) {
		return nullptr;
	}

	return fdopen(fd[0], "r");
}

int exec_wait(int pid)
{
	int wstatus;
	if(waitpid(pid, &wstatus, 0) < 0) {
		return -1;
	}
	return WEXITSTATUS(wstatus);
}
