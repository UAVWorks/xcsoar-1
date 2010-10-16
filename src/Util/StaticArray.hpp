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

#ifndef XCSOAR_STATIC_ARRAY_HPP
#define XCSOAR_STATIC_ARRAY_HPP

#include <assert.h>
#include <algorithm>

/**
 * An array with a maximum size known at compile time.  It keeps track
 * of the actual length at runtime.
 */
template<class T, unsigned max>
class StaticArray {
public:
  static const unsigned MAX_SIZE = max;

protected:
  unsigned the_size;
  T data[max];

public:
  StaticArray():the_size(0) {}

  /**
   * Returns the number of allocated elements.
   */
  unsigned size() const {
    return the_size;
  }

  bool empty() const {
    return the_size == 0;
  }

  bool full() const {
    return the_size == max;
  }

  /**
   * Empties this array, but does not destruct its elements.
   */
  void clear() {
    the_size = 0;
  }

  /**
   * Returns one element.  No bounds checking.
   */
  T &operator[](unsigned i) {
    assert(i < size());

    return data[i];
  }

  /**
   * Returns one constant element.  No bounds checking.
   */
  const T &operator[](unsigned i) const {
    assert(i < size());

    return data[i];
  }

  T *begin() {
    return data;
  }

  const T *begin() const {
    return data;
  }

  T *end() {
    return data + the_size;
  }

  const T *end() const {
    return data + the_size;
  }

  T &last() {
    assert(the_size > 0);

    return data[the_size - 1];
  }

  const T &last() const {
    assert(the_size > 0);

    return data[the_size - 1];
  }

  bool contains(const T &value) const {
    return std::find(begin(), end(), value) != end();
  }

  /**
   * Append an element at the end of the array, increasing the length
   * by one.  No bounds checking.
   */
  void append(const T &value) {
    assert(!full());

    data[the_size++] = value;
  }

  /**
   * Increase the length by one and return a pointer to the new
   * element, to be modified by the caller.  No bounds checking.
   */
  T &append() {
    assert(!full());

    return data[the_size++];
  }

  /**
   * Like append(), but checks if the array is already full (returns
   * false in this case).
   */
  bool checked_append(const T &value) {
    if (full())
      return false;

    append(value);
    return true;
  }
};

#endif
