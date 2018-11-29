#include "config.h"
#include "Parser.hxx"
#include "system/Error.hxx"
#include "util/ScopeExit.hxx"
#include "event/Loop.hxx"
#include "event/Call.hxx"

#include <cinttypes>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>

namespace Ytdl {

std::unique_ptr<YtdlProcess>
YtdlProcess::Invoke(Yajl::Handle &handle, const char *url, PlaylistMode mode)
{
	int pipefd[2];
	if (pipe2(pipefd, O_CLOEXEC) < 0) {
		throw MakeErrno("Failed to create pipe");
	}

	AtScopeExit(&pipefd) {
		if (pipefd[0] != -1) {
			close(pipefd[0]);
		}
		close(pipefd[1]);
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

	auto process = std::make_unique<YtdlProcess>(handle, pipefd[0], pid);

	pipefd[0] = -1; // sentinel to prevent closing AtExit

	return process;
}

YtdlProcess::~YtdlProcess()
{
	close(fd);

	if (pid != -1) {
		waitpid(pid, nullptr, 0);
	}
}

bool
YtdlProcess::Process()
{
	uint8_t buffer[0x80];
	int res = read(fd, buffer, sizeof(buffer));
	if (res < 0) {
		throw MakeErrno("failed to read from pipe");
	} else if (res > 0) {
		handle.Parse(buffer, res);
		return true;
	}

	handle.CompleteParse();

	if (waitpid(pid, &res, 0) < 0) {
		throw MakeErrno("failed to wait on youtube-dl process");
	}

	pid = -1;

	if (res) {
		throw FormatRuntimeError("youtube-dl exited with code %d", res);
	}

	return false;
}

bool
YtdlMonitor::OnSocketReady(unsigned flags) noexcept
{
	try {
		// TODO: repeatedly call Process and wait for EWOULDBLOCK?
		if (process->Process()) {
			return true;
		} else {
			handler.OnComplete(this);
			return false;
		}
	} catch (...) {
		handler.OnError(this, std::current_exception());
		return false;
	}
}

std::unique_ptr<YtdlMonitor>
Invoke(Yajl::Handle &handle, const char *url, PlaylistMode mode, EventLoop &loop, YtdlHandler &handler)
{
	auto process = YtdlProcess::Invoke(handle, url, mode);

	std::unique_ptr<YtdlMonitor> monitor = std::make_unique<YtdlMonitor>(handler, std::move(process), loop);
	BlockingCall(loop, [&] {
		monitor->ScheduleRead();
	});

	return monitor;
}

void
BlockingInvoke(Yajl::Handle &handle, const char *url, PlaylistMode mode)
{
	auto process = YtdlProcess::Invoke(handle, url, mode);

	while (process->Process()) {}
}

} // namespace Ytdl
