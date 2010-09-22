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

#ifndef XCSOAR_PERIOD_CLOCK_HPP
#define XCSOAR_PERIOD_CLOCK_HPP

#ifdef HAVE_POSIX
#include <time.h>
#else /* !HAVE_POSIX */
#include <windows.h>
#endif /* !HAVE_POSIX */

#ifdef __CYGWIN__
  #define CLOCK_MONOTONIC CLOCK_REALTIME
#endif

/**
 * This is a stopwatch which saves the timestamp of an even, and can
 * check whether a specified time span has passed since then.
 */
class PeriodClock {
protected:
#ifdef HAVE_POSIX
  typedef unsigned stamp_t;
#else /* !HAVE_POSIX */
  typedef DWORD stamp_t;
#endif /* !HAVE_POSIX */

private:
  stamp_t last;

public:
  /**
   * Initializes the object, setting the last time stamp to "0",
   * i.e. a check() will always succeed.  If you do not want this
   * default behaviour, call update() immediately after creating the
   * object.
   */
  PeriodClock():last(0) {}

protected:
  static stamp_t get_now() {
#ifdef HAVE_POSIX
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#else /* !HAVE_POSIX */
    return ::GetTickCount();
#endif /* !HAVE_POSIX */
  }

public:
  /**
   * Resets the clock.
   */
  void reset() {
    last = 0;
  }

  /**
   * Returns the number of milliseconds elapsed since the last
   * update().  Returns -1 if update() was never called.
   */
  int elapsed() const {
    if (last == 0)
      return -1;

    return get_now() - last;
  }

  /**
   * Checks whether the specified duration has passed since the last
   * update.
   *
   * @param duration the duration in milliseconds
   */
  bool check(unsigned duration) const {
    return get_now() >= last + duration;
  }

  /**
   * Updates the time stamp, setting it to the current clock.
   */
  void update() {
    last = get_now();
  }

  /**
   * Updates the time stamp, setting it to the current clock plus the
   * specified offset.
   */
  void update_offset(int offset) {
    update();
    last += offset;
  }

  /**
   * Checks whether the specified duration has passed since the last
   * update.  If yes, it updates the time stamp.
   *
   * @param duration the duration in milliseconds
   */
  bool check_update(unsigned duration) {
    stamp_t now = get_now();
    if (now >= last + duration) {
      last = now;
      return true;
    } else
      return false;
  }

  /**
   * Checks whether the specified duration has passed since the last
   * update.  After that, it updates the time stamp.
   *
   * @param duration the duration in milliseconds
   */
  bool check_always_update(unsigned duration) {
    stamp_t now = get_now();
    bool ret = now > last + duration;
    last = now;
    return ret;
  }
};

#endif
