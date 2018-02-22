#include "config.h"
#include "Parser.hxx"
#include "util/ScopeExit.hxx"

#include <cinttypes>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

namespace Ytdl {

void
Invoke(Yajl::Handle &handle, const char *url, PlaylistMode mode)
{
	int pipefd[2];
	if (pipe2(pipefd, O_CLOEXEC) < 0) {
		throw std::runtime_error("Failed to create pipe");
	}

	AtScopeExit(pipefd) {
		close(pipefd[0]);
		if (pipefd[1] != -1) {
			close(pipefd[1]);
		}
	};

	int pid = fork();
	if (pid < 0) {
		throw std::runtime_error("Failed to fork()");
	}

	if (!pid) {
		close(pipefd[0]);
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
			close(pipefd[1]);
			_exit(EXIT_FAILURE);
		}
	}

	close(pipefd[1]);
	pipefd[1] = -1; // sentinel to prevent closing AtExit

	int ret;
	uint8_t buffer[0x80];
	do {
		ret = read(pipefd[0], buffer, sizeof(buffer));
		if (ret < 0) {
			throw std::runtime_error("failed to read from pipe");
		} else if (ret > 0) {
			handle.Parse(buffer, ret);
		}
	} while (ret > 0);

	handle.CompleteParse();

	if (waitpid(pid, &ret, 0) < 0) {
		throw std::runtime_error("failed to wait on youtube-dl process");
	}

	if (ret) {
		throw FormatRuntimeError("youtube-dl exited with code %d", ret);
	}
}

} // namespace Ytdl
