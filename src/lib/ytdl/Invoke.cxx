#include "config.h"
#include "Parser.hxx"
#include "system/Error.hxx"
#include "util/ScopeExit.hxx"

#include <cinttypes>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>

namespace Ytdl {

void
Invoke(Yajl::Handle &handle, const char *url, PlaylistMode mode)
{
	int pipefd[2];
	if (pipe2(pipefd, O_CLOEXEC) < 0) {
		throw MakeErrno("Failed to create pipe");
	}

	AtScopeExit(pipefd) {
		close(pipefd[0]);
		if (pipefd[1] != -1) {
			close(pipefd[1]);
		}
	};

	// block all signals while forking child
	int res;
	sigset_t signals_new, signals_old;
	sigfillset(&signals_new);
	if ((res = pthread_sigmask(SIG_SETMASK, &signals_new, &signals_old))) {
		throw MakeErrno(res, "Failed to block signals");
	}

	int pid = fork();

	if (!pid) {
		// restore all signal handlers to default
		struct sigaction act;
		act.sa_handler = SIG_DFL;
		act.sa_flags = 0;
		sigemptyset(&act.sa_mask);
		for (int i = 0; i < NSIG; i++) {
			sigaction(i, &act, nullptr);
		}

		if (pthread_sigmask(SIG_SETMASK, &signals_old, nullptr)) {
			_exit(EXIT_FAILURE);
		}

		if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
			_exit(EXIT_FAILURE);
		}

		const char *playlist_flag;
		switch (mode) {
			case PlaylistMode::SINGLE:
				playlist_flag = "--no-playlist";
				break;
			case PlaylistMode::FLAT:
				playlist_flag = "--flat-playlist";
				break;
			case PlaylistMode::FULL:
				playlist_flag = "--yes-playlist";
				break;
		}

		if (execlp("youtube-dl", "youtube-dl",
			"-Jf", "bestaudio/best", playlist_flag, url, nullptr) < 0)
		{
			_exit(EXIT_FAILURE);
		}
	}

	// restore blocked signals
	if ((res = pthread_sigmask(SIG_SETMASK, &signals_old, nullptr))) {
		throw MakeErrno(res, "Failed to unblock signals");
	}

	if (pid < 0) {
		throw MakeErrno("Failed to fork()");
	}

	close(pipefd[1]);
	pipefd[1] = -1; // sentinel to prevent closing AtExit

	uint8_t buffer[0x80];
	do {
		res = read(pipefd[0], buffer, sizeof(buffer));
		if (res < 0) {
			throw MakeErrno("failed to read from pipe");
		} else if (res > 0) {
			handle.Parse(buffer, res);
		}
	} while (res > 0);

	handle.CompleteParse();

	if (waitpid(pid, &res, 0) < 0) {
		throw MakeErrno("failed to wait on youtube-dl process");
	}

	if (res) {
		throw FormatRuntimeError("youtube-dl exited with code %d", res);
	}
}

} // namespace Ytdl
