/*
 * Copyright (C) 2010 Max Kellermann <max@duempel.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ALLOCATED_ARRAY_HPP
#define ALLOCATED_ARRAY_HPP

#include "gcc.h"

#include <assert.h>
#include <algorithm>

/**
 * An array allocated on the heap with a length determined at runtime.
 */
template<class T>
class AllocatedArray {
public:
  typedef size_t size_type;

protected:
  size_type the_size;
  T *restrict data;

public:
  typedef T *iterator;
  typedef const T *const_iterator;

public:
  constexpr AllocatedArray():the_size(0), data(nullptr) {}

  explicit AllocatedArray(size_type _size)
    :the_size(_size), data(new T[the_size]) {
    assert(size() == 0 || data != nullptr);
  }

  explicit AllocatedArray(const AllocatedArray &other)
    :the_size(other.size()), data(new T[the_size]) {
    assert(size() == 0 || data != nullptr);
    assert(other.size() == 0 || other.data != nullptr);

    std::copy(other.data, other.data + the_size, data);
  }

  explicit AllocatedArray(AllocatedArray &&other)
    :the_size(other.the_size), data(other.data) {
    other.the_size = 0;
    other.data = nullptr;
  }

  ~AllocatedArray() {
    delete[] data;
  }

  AllocatedArray &operator=(const AllocatedArray &other) {
    assert(size() == 0 || data != nullptr);
    assert(other.size() == 0 || other.data != nullptr);

    if (&other == this)
      return *this;

    ResizeDiscard(other.size());
    std::copy(other.begin(), other.end(), data);
    return *this;
  }

  AllocatedArray &operator=(AllocatedArray &&other) {
    std::swap(the_size, other.the_size);
    std::swap(data, other.data);
    return *this;
  }

  /**
   * Returns true if no memory was allocated so far.
   */
  constexpr bool empty() const {
    return the_size == 0;
  }

  /**
   * Returns the number of allocated elements.
   */
  constexpr size_type size() const {
    return the_size;
  }

  /**
   * Returns one element.  No bounds checking.
   */
  T &operator[](size_type i) {
    assert(i < size());

    return data[i];
  }

  /**
   * Returns one constant element.  No bounds checking.
   */
  const T &operator[](size_type i) const {
    assert(i < size());

    return data[i];
  }

  iterator begin() {
    return data;
  }

  constexpr const_iterator begin() const {
    return data;
  }

  iterator end() {
    return data + the_size;
  }

  constexpr const_iterator end() const {
    return data + the_size;
  }

  /**
   * Resizes the array, discarding old data.
   */
  void ResizeDiscard(size_type _size) {
    if (_size == the_size)
      return;

    delete[] data;
    the_size = _size;
    data = new T[the_size];

    assert(size() == 0 || data != nullptr);
  }

  /**
   * Grows the array to the specified size, discarding old data.
   * Similar to ResizeDiscard(), but will never shrink the array to
   * avoid expensive heap operations.
   */
  void GrowDiscard(size_type _size) {
    if (_size > the_size)
      ResizeDiscard(_size);
  }

  /**
   * Grows the array to the specified size, preserving the value of a
   * range of elements, starting from the beginning.
   */
  void GrowPreserve(size_type _size, size_type preserve) {
    if (_size <= the_size)
      return;

    T *new_data = new T[_size];
    assert(_size == 0 || new_data != nullptr);

    std::move(data, data + preserve, new_data);

    delete[] data;
    data = new_data;
    the_size = _size;
  }
};

#endif
