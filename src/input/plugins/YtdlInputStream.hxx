#ifndef MPD_INPUT_YTDL_STREAM_HXX
#define MPD_INPUT_YTDL_STREAM_HXX

#include "../InputStream.hxx"
#include "../Handler.hxx"
#include "lib/ytdl/Invoke.hxx"

class Tag;

class YtdlInputStream : public InputStream, public InputStreamHandler, public Ytdl::YtdlHandler {
	std::unique_ptr<Ytdl::InvokeContext> context;
	std::unique_ptr<Tag> tag;
	InputStreamPtr inner;
	std::exception_ptr pending_exception;

	void SyncFields() noexcept;

public:
	YtdlInputStream(const char *_uri, Mutex &_mutex, EventLoop &event_loop) noexcept;

	virtual ~YtdlInputStream() noexcept;
	virtual void Check();
	virtual void Update() noexcept;
	virtual void Seek(offset_type offset);

	gcc_pure
	virtual bool IsEOF() noexcept;
	virtual std::unique_ptr<Tag> ReadTag();

	gcc_pure
	virtual bool IsAvailable() noexcept;

	gcc_nonnull_all
	virtual size_t Read(void *ptr, size_t size);

	void OnComplete(Ytdl::YtdlMonitor* monitor);
	void OnError(Ytdl::YtdlMonitor* monitor, std::exception_ptr e);

	virtual void OnInputStreamReady() noexcept;
	virtual void OnInputStreamAvailable() noexcept;
};

#endif
