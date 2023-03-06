// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ProxyInputStream.hxx"
#include "tag/Tag.hxx"

#include <utility>

ProxyInputStream::ProxyInputStream(InputStreamPtr _input) noexcept
	:InputStream(_input->GetUriView(), _input->mutex),
	 input(std::move(_input))
{
	assert(input);

	input->SetHandler(this);
}

ProxyInputStream::~ProxyInputStream() noexcept = default;

void
ProxyInputStream::SetInput(InputStreamPtr _input) noexcept
{
	assert(!input);
	assert(_input);

	input = std::move(_input);
	input->SetHandler(this);

	/* this call wakes up client threads if the new input is
	   ready */
	CopyAttributes();

	set_input_cond.notify_one();
}

void
ProxyInputStream::CopyAttributes()
{
	assert(input);

	if (input->IsReady()) {
		if (!IsReady()) {
			if (input->HasMimeType())
				SetMimeType(input->GetMimeType());

			size = input->KnownSize()
				? input->GetSize()
				: UNKNOWN_SIZE;

			seekable = input->IsSeekable();
			SetReady();
		}

		offset = input->GetOffset();
	}
}

void
ProxyInputStream::Check()
{
	if (input)
		input->Check();
}

void
ProxyInputStream::Update() noexcept
{
	if (!input)
		return;

	input->Update();
	CopyAttributes();
}

void
ProxyInputStream::Seek(std::unique_lock<Mutex> &lock,
		       offset_type new_offset)
{
	set_input_cond.wait(lock, [this]{ return !!input; });

	input->Seek(lock, new_offset);
	CopyAttributes();
}

bool
ProxyInputStream::IsEOF() const noexcept
{
	return input && input->IsEOF();
}

std::unique_ptr<Tag>
ProxyInputStream::ReadTag() noexcept
{
	if (!input)
		return nullptr;

	return input->ReadTag();
}

bool
ProxyInputStream::IsAvailable() const noexcept
{
	return input && input->IsAvailable();
}

size_t
ProxyInputStream::Read(std::unique_lock<Mutex> &lock,
		       void *ptr, size_t read_size)
{
	set_input_cond.wait(lock, [this]{ return !!input; });

	size_t nbytes = input->Read(lock, ptr, read_size);
	CopyAttributes();
	return nbytes;
}
