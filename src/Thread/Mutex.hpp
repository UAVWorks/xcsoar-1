/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009

	M Roberts (original release)
	Robin Birch <robinb@ruffnready.co.uk>
	Samuel Gisiger <samuel.gisiger@triadis.ch>
	Jeff Goodenough <jeff@enborne.f2s.com>
	Alastair Harrison <aharrison@magic.force9.co.uk>
	Scott Penrose <scottp@dd.com.au>
	John Wharington <jwharington@gmail.com>
	Lars H <lars_hn@hotmail.com>
	Rob Dunning <rob@raspberryridgesheepfarm.com>
	Russell King <rmk@arm.linux.org.uk>
	Paolo Ventafridda <coolwind@email.it>
	Tobias Lohner <tobias@lohner-net.de>
	Mirek Jezek <mjezek@ipplc.cz>
	Max Kellermann <max@duempel.org>
	Tobias Bieniek <tobias.bieniek@gmx.de>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

/**
 * This file is about mutexes.
 *
 * A mutex lock is also known as a mutually exclusive lock. This type of lock
 * is provided by many threading systems as a means of synchronization.
 * Basically, it is only possible for one thread to grab a mutex at a time:
 * if two threads try to grab a mutex, only one succeeds. The other thread
 * has to wait until the first thread releases the lock; it can then grab the
 * lock and continue operation.
 * @file Mutex.hpp
 */

#ifndef XCSOAR_THREAD_MUTEX_HXX
#define XCSOAR_THREAD_MUTEX_HXX

#include "Util/NonCopyable.hpp"

#ifdef HAVE_POSIX
#include <pthread.h>
#else
#include <windows.h>
#endif

#ifndef NDEBUG
#include "Thread/Local.hpp"
extern ThreadLocalInteger thread_locks_held;
#endif

#ifdef __CYGWIN__
#define PTHREAD_MUTEX_RECURSIVE_NP PTHREAD_MUTEX_RECURSIVE
#endif

/**
 * This class wraps an OS specific mutex.  It is an object which one
 * thread can wait for, and another thread can wake it up.
 */
class Mutex : private NonCopyable {
#ifdef HAVE_POSIX
  pthread_mutex_t mutex;
#else
  CRITICAL_SECTION handle;
#endif

public:
  /**
   * Initializes the Mutex
   */
  Mutex() {
#ifdef HAVE_POSIX
    /* the XCSoar code assumes that recursive locking of a Mutex is
       legal */
    pthread_mutexattr_t recursive;
    pthread_mutexattr_init(&recursive);
    pthread_mutexattr_settype(&recursive, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&mutex, &recursive);
    pthread_mutexattr_destroy(&recursive);
#else
    ::InitializeCriticalSection(&handle);
#endif
  }

  /**
   * Deletes the Mutex
   */
  ~Mutex() {
#ifdef HAVE_POSIX
    pthread_mutex_destroy(&mutex);
#else
    ::DeleteCriticalSection(&handle);
#endif
  }

public:
  /**
   * Locks the Mutex
   */
  void Lock() {
#ifdef HAVE_POSIX
    pthread_mutex_lock(&mutex);
#else
    EnterCriticalSection(&handle);
#endif

#ifndef NDEBUG
    ++thread_locks_held;
#endif
  };

  /**
   * Tries to lock the Mutex
   */
  bool TryLock() {
#ifdef HAVE_POSIX
    if (pthread_mutex_trylock(&mutex) != 0)
      return false;
#else
    if (TryEnterCriticalSection(&handle) == 0)
      return false;
#endif

#ifndef NDEBUG
    ++thread_locks_held;
#endif
    return true;
  };

  /**
   * Unlocks the Mutex
   */
  void Unlock() {
#ifdef HAVE_POSIX
    pthread_mutex_unlock(&mutex);
#else
    LeaveCriticalSection(&handle);
#endif

#ifndef NDEBUG
    --thread_locks_held;
#endif
  }
};

/**
 * A class for an easy and clear way of handling mutexes
 *
 * Creating a ScopeLock instance locks the given Mutex, while
 * destruction of the ScopeLock leads to unlocking the Mutex.
 *
 * Usage: Create a ScopeLock at the beginning of a function and
 * after function is executed the ScopeLock will destroy itself
 * and unlock the Mutex again.
 * @author JMW
 */
class ScopeLock : private NonCopyable {
public:
  ScopeLock(Mutex& the_mutex):scope_mutex(the_mutex) {
    scope_mutex.Lock();
  };
  ~ScopeLock() {
    scope_mutex.Unlock();
  }
private:
  Mutex &scope_mutex;
};

#endif
