#include "YtdlInputStream.hxx"
#include "CurlInputPlugin.hxx"
#include "tag/Tag.hxx"

YtdlInputStream::YtdlInputStream(const char *_uri, Mutex &_mutex, EventLoop &event_loop) noexcept
	:InputStream(_uri, _mutex)
{
	try {
		context = Ytdl::InvokeContext::Invoke(_uri, Ytdl::PlaylistMode::SINGLE, event_loop, *this);
	} catch (...) {
		pending_exception = std::current_exception();
	}
}

YtdlInputStream::~YtdlInputStream() noexcept {
}

void YtdlInputStream::SyncFields() noexcept {
	seekable = inner->IsSeekable();
	size = inner->GetSize();
	offset = inner->GetOffset();
}

void YtdlInputStream::Check() {
	if (pending_exception != nullptr) {
		std::exception_ptr ex = pending_exception;
		pending_exception = nullptr;
		std::rethrow_exception(ex);
	}

	if (inner != nullptr) {
		inner->Check();
	}
}

void YtdlInputStream::Update() noexcept {
	if (inner != nullptr) {
		inner->Update();
		if (!ready && inner->IsReady()) {
			SetMimeType(inner->GetMimeType());
			SetReady();
		}
		if (inner->IsReady()) {
			SyncFields();
		}
	}
}

void YtdlInputStream::Seek(offset_type by_offset) {
	if (inner != nullptr) {
		inner->Seek(by_offset);
	}
}

gcc_pure
bool YtdlInputStream::IsEOF() noexcept {
	if (inner != nullptr) {
		return inner->IsEOF();
	} else {
		return false;
	}
}

std::unique_ptr<Tag> YtdlInputStream::ReadTag() {
	if (tag != nullptr) {
		return std::move(tag);
	} else if (inner != nullptr) {
		return inner->ReadTag();
	} else {
		return nullptr;
	}
}

gcc_pure
bool YtdlInputStream::IsAvailable() noexcept {
	if (inner != nullptr) {
		return inner->IsAvailable();
	} else {
		return pending_exception != nullptr;
	}
}

gcc_nonnull_all
size_t YtdlInputStream::Read(void *ptr, size_t sz) {
	if (inner != nullptr) {
		size_t res = inner->Read(ptr, sz);
		SyncFields();
		return res;
	} else {
		throw std::runtime_error("youtube-dl stream not ready for reading");
	}
}

void YtdlInputStream::OnComplete(Ytdl::YtdlMonitor* monitor) {
	const std::lock_guard<Mutex> protect(mutex);
	try {
		tag = context->GetMetadata().GetTagBuilder().CommitNew();
		inner = OpenCurlInputStream(context->GetMetadata().GetURL().c_str(),
			context->GetMetadata().GetHeaders(), mutex);
		inner->SetHandler(this);
	} catch (...) {
		pending_exception = std::current_exception();
	}
	context = nullptr;
}

void YtdlInputStream::OnError(Ytdl::YtdlMonitor* monitor, std::exception_ptr e) {
	const std::lock_guard<Mutex> protect(mutex);
	pending_exception = e;
	context = nullptr;
}

void YtdlInputStream::OnInputStreamReady() noexcept {
	InvokeOnReady();
}

void YtdlInputStream::OnInputStreamAvailable() noexcept {
	InvokeOnAvailable();
}
