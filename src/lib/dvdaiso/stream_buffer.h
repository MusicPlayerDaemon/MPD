/*
* MPD DVD-Audio Decoder plugin
* Copyright (c) 2014 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
*
* DVD-Audio Decoder is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* DVD-Audio Decoder is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef _STREAM_BUFFER_H_INCLUDED
#define _STREAM_BUFFER_H_INCLUDED

#include <cstdint>

template <class T, class I> class stream_buffer_t {
	T*  bank[2];
	int bank_id;
	I bank_base;
	I bank_size;
	I min_read_size;
	I avg_write_size;
	T* read_ptr[2];
	T* read_end[2];
	bool banks_switched;
public:
	stream_buffer_t(void) {
		bank[0] = bank[1] = (T*)0;
		bank_size = 0;
	}
	~stream_buffer_t(void) {
		free();
	}
	bool init(I _bank_size, I _min_read_size, I _avg_write_size) {
		free();
		bank[0] = (T*)malloc((size_t)(_bank_size + _min_read_size) * sizeof(T));
		bank[1] = (T*)malloc((size_t)(_bank_size + _min_read_size) * sizeof(T));
		if (bank[0] == (T*)0 || bank[1] == (T*)0) {
			free();
			return false;
		}
		bank_id = 0;
		bank_base = _min_read_size;
		bank_size = _bank_size;
		min_read_size  = _min_read_size;
		avg_write_size = _avg_write_size;
		read_ptr[0] = read_end[0] = bank[0] + bank_base;
		read_ptr[1] = read_end[1] = bank[1] + bank_base;
		banks_switched = false;
 		return true;
	}
	void reinit() {
		bank_id = 0;
		read_ptr[0] = read_end[0] = bank[0] + bank_base;
		read_ptr[1] = read_end[1] = bank[1] + bank_base;
	}
	void free(void) {
		if (bank[0] != (T*)0) {
			::free(bank[0]);
			bank[0] = (T*)0;
		}
		if (bank[1] != (T*)0) {
			::free(bank[1]);
			bank[1] = (T*)0;
		}
		bank_size = 0;
	}
	int get_next_bank(int bank_id) {
		return ~bank_id & 1;
	}
	I get_bank_size(void) {
		return bank_size;
	}
	T* get_read_ptr(void) {
		return read_ptr[bank_id];
	}
	T* move_read_ptr(I size) {
		T* new_ptr = (T*)0;
		if (read_ptr[bank_id] + min_read_size + size <= read_end[bank_id])
			read_ptr[bank_id] += size;
		else {
			int next_bank_id = get_next_bank(bank_id);
			T* read_ptr_rest = read_ptr[bank_id] + size;
			I read_size_rest = read_end[bank_id] - read_ptr_rest;
			if (read_ptr[bank_id] + size < read_end[bank_id])
				memcpy(bank[next_bank_id] + bank_base - read_size_rest, read_ptr[bank_id] + size, read_size_rest);
			read_ptr[next_bank_id] = bank[next_bank_id] + bank_base - read_size_rest;
			read_ptr[bank_id] = read_end[bank_id] = bank[bank_id] + bank_base;
			switch_banks();
		}
		new_ptr = read_ptr[bank_id];
		return new_ptr;
	}
	I get_read_size(void) {
		I read_size;
		read_size = read_end[bank_id] - read_ptr[bank_id];
		return read_size;
	}
	I set_read_size(I size) {
		I read_size = 0;
		if (read_ptr[bank_id] + size <= bank[bank_id] + bank_base + bank_size) {
			read_end[bank] = read_ptr[bank_id] + read_size; 
			read_size = size;
		}
		return read_size;
	}
	T* get_write_ptr(void) {
		int bank_wr = get_next_bank(bank_id);
		return read_end[bank_wr];
	}
	T* move_write_ptr(I size) {
		T* new_ptr = (T*)0;
		int bank_wr = get_next_bank(bank_id);
		if (read_end[bank_wr] + size <= bank[bank_wr] + bank_base + bank_size) {
			read_end[bank_wr] += size;
			new_ptr = read_end[bank_wr];
		}
		return new_ptr;
	}
	I get_write_size(void) {
		I write_size;
		int bank_wr = get_next_bank(bank_id);
		write_size = bank[bank_wr] + bank_base + bank_size - read_end[bank_wr];
		write_size = write_size < avg_write_size ? write_size : avg_write_size;
		return write_size;
	}
	bool is_ready_to_write() {
		return avg_write_size <= get_write_size();
	}
	bool needs_data() {
		if (banks_switched) {
			banks_switched = false;
			return true;
		}
		return false;
	}
private:
	void switch_banks() {
		bank_id = ~bank_id & 1;
		banks_switched = true;
	}
};

#endif
