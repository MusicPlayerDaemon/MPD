/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#ifndef MPD_OUTPUT_WRAPPER_HXX
#define MPD_OUTPUT_WRAPPER_HXX

#include "util/Cast.hxx"

struct ConfigBlock;

template<class T>
struct AudioOutputWrapper {
	static T &Cast(AudioOutput &ao) {
		return ContainerCast(ao, &T::base);
	}

	static AudioOutput *Init(const ConfigBlock &block, Error &error) {
		T *t = T::Create(block, error);
		return t != nullptr
			? &t->base
			: nullptr;
	}

	static void Finish(AudioOutput *ao) {
		T *t = &Cast(*ao);
		delete t;
	}

	static bool Enable(AudioOutput *ao, Error &error) {
		T &t = Cast(*ao);
		return t.Enable(error);
	}

	static void Disable(AudioOutput *ao) {
		T &t = Cast(*ao);
		t.Disable();
	}

	static bool Open(AudioOutput *ao, AudioFormat &audio_format,
			 Error &error) {
		T &t = Cast(*ao);
		return t.Open(audio_format, error);
	}

	static void Close(AudioOutput *ao) {
		T &t = Cast(*ao);
		t.Close();
	}

	gcc_pure
	static unsigned Delay(AudioOutput *ao) {
		T &t = Cast(*ao);
		return t.Delay();
	}

	gcc_pure
	static void SendTag(AudioOutput *ao, const Tag &tag) {
		T &t = Cast(*ao);
		t.SendTag(tag);
	}

	static size_t Play(AudioOutput *ao, const void *chunk, size_t size,
			   Error &error) {
		T &t = Cast(*ao);
		return t.Play(chunk, size, error);
	}

	static void Drain(AudioOutput *ao) {
		T &t = Cast(*ao);
		t.Drain();
	}

	static void Cancel(AudioOutput *ao) {
		T &t = Cast(*ao);
		t.Cancel();
	}

	gcc_pure
	static bool Pause(AudioOutput *ao) {
		T &t = Cast(*ao);
		return t.Pause();
	}
};

#endif
