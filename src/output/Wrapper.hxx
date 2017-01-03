/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include <chrono>

struct ConfigBlock;

template<class T>
struct AudioOutputWrapper {
	static T &Cast(AudioOutput &ao) {
		return ContainerCast(ao, &T::base);
	}

	static AudioOutput *Init(const ConfigBlock &block) {
		T *t = T::Create(block);
		return &t->base;
	}

	static void Finish(AudioOutput *ao) {
		T *t = &Cast(*ao);
		delete t;
	}

	static void Enable(AudioOutput *ao) {
		T &t = Cast(*ao);
		t.Enable();
	}

	static void Disable(AudioOutput *ao) {
		T &t = Cast(*ao);
		t.Disable();
	}

	static void Open(AudioOutput *ao, AudioFormat &audio_format) {
		T &t = Cast(*ao);
		t.Open(audio_format);
	}

	static void Close(AudioOutput *ao) {
		T &t = Cast(*ao);
		t.Close();
	}

	gcc_pure
	static std::chrono::steady_clock::duration Delay(AudioOutput *ao) {
		T &t = Cast(*ao);
		return t.Delay();
	}

	gcc_pure
	static void SendTag(AudioOutput *ao, const Tag &tag) {
		T &t = Cast(*ao);
		t.SendTag(tag);
	}

	static size_t Play(AudioOutput *ao, const void *chunk, size_t size) {
		T &t = Cast(*ao);
		return t.Play(chunk, size);
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
