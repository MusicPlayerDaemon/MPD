#include "SockOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "../Timer.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "fs/FileInfo.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"
#include "open.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>

class SockOutput final : AudioOutput {
  const AllocatedPath path;
  std::string path_utf8;

  int fd = -1;
  bool created = false;
  Timer *timer;

public:
  SockOutput(const ConfigBlock &block);

  ~SockOutput() {
    CloseSock();
  }

  static AudioOutput *Create(EventLoop &,
            const ConfigBlock &block) {
    return new SockOutput(block);
  }

private:
  void Create();
  void Check();
  void Delete();

  void OpenSock();
  void CloseSock();

  void Open(AudioFormat &audio_format) override;
  void Close() noexcept override;

  std::chrono::steady_clock::duration Delay() const noexcept override;
  size_t Play(const void *chunk, size_t size) override;
  void Cancel() noexcept override;
};

static constexpr Domain sock_output_domain("sock_output");

SockOutput::SockOutput(const ConfigBlock &block)
  :AudioOutput(0),
   path(block.GetPath("path"))
{
  if (path.IsNull())
    throw std::runtime_error("No \"path\" parameter specified");

  path_utf8 = path.ToUTF8();

  OpenSock();
}

inline void
SockOutput::Delete()
{
  FormatDebug(sock_output_domain,
        "Removing Unix Socket \"%s\"", path_utf8.c_str());

  try {
    RemoveFile(path);
  } catch (...) {
    LogError(std::current_exception(), "Could not remove Unix Socket");
    return;
  }

  created = false;
}

void
SockOutput::CloseSock()
{
  close(fd);
}

inline void
SockOutput::Create()
{
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));

  if (path.length + 1 > sizeof(addr.sun_path))
    throw FormatRuntimeError("Cannot create Unix Socket, path is too long: \"%s\"",
          path_utf8.c_str());

  // TODO Check errors

  fd = socket(AF_UNIX, SOCK_STREAM, 0);

  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path)-1);
  bind(fd, (struct sockaddr *) &addr, sizeof(addr));

  created = true;
}

inline void
SockOutput::Check()
{
  struct stat st;
  if (!StatFile(path, st)) {
    if (errno == ENOENT) {
      /* Path doesn't exist */
      Create();
      return;
    }

    throw FormatErrno("Failed to stat Unix Socket \"%s\"",
          path_utf8.c_str());
  }

  if (!S_ISSOCK(st.st_mode))
    throw FormatRuntimeError("\"%s\" already exists, but is not a Unix Socket",
          path_utf8.c_str());
}

inline void
SockOutput::OpenSock()
try {
	Check();

  // TODO Input and output
} catch (...) {
	CloseSock();
	throw;
}

void
SockOutput::Open(AudioFormat &audio_format)
{
  timer = new Timer(audio_format);
}

void
SockOutput::Close() noexcept
{
  delete timer;
}

void
SockOutput::Cancel() noexcept
{
  timer->Reset();

  // TODO Flush socket
}

std::chrono::steady_clock::duration
SockOutput::Delay() const noexcept
{
	return timer->IsStarted()
		? timer->GetDelay()
		: std::chrono::steady_clock::duration::zero();
}

size_t
SockOutput::Play(const void *chunk, size_t size)
{
	if (!timer->IsStarted())
		timer->Start();
	timer->Add(size);

	while (true) {
		// TODO Write to all outputs
	}
}

const struct AudioOutputPlugin sock_output_plugin = {
	"sock",
	nullptr,
	&SockOutput::Create,
	nullptr,
};
