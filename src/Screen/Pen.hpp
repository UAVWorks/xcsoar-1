/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2010 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

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

#ifndef XCSOAR_SCREEN_PEN_HPP
#define XCSOAR_SCREEN_PEN_HPP

#include "Util/NonCopyable.hpp"
#include "Screen/Color.hpp"

/**
 * A pen draws lines and borders.
 */
class Pen
#ifndef ENABLE_SDL
  : private NonCopyable
#endif
{
public:
  #ifdef ENABLE_SDL
  enum style {
    SOLID,
    DASH,
    BLANK
  };
  #else
  enum style {
    SOLID = PS_SOLID,
    DASH = PS_DASH,
    BLANK = PS_NULL
  };
  #endif

protected:
  #ifdef ENABLE_SDL
  unsigned width;
  Color color;
  #else
  HPEN pen;
  #endif

public:
  #ifdef ENABLE_SDL
  Pen():width(0) {}
  Pen(enum style style, unsigned _width, const Color _color)
    :width(_width), color(_color) {} // XXX style
  Pen(unsigned _width, const Color _color)
    :width(_width), color(_color) {}
  #else /* !ENABLE_SDL */
  /** Base Constructor for the Pen class */
  Pen() : pen(NULL) {}
  /**
   * Constructor that creates a Pen object, based on the given parameters
   * @param style Line style (SOLID, DASH, BLANK)
   * @param width Width of the line/Pen
   * @param c Color of the Pen
   */
  Pen(enum style style, unsigned width, const Color c):pen(NULL) {
    set(style, width, c);
  }
  /**
   * Constructor that creates a solid Pen object, based on the given parameters
   * @param width Width of the line/Pen
   * @param c Color of the Pen
   */
  Pen(unsigned width, Color c):pen(NULL) {
    set(width, c);
  }
  #endif /* !ENABLE_SDL */

  /** Destructor */
  ~Pen() { reset(); }

public:
  /**
   * Sets the Pens parameters to the given values
   * @param style Line style (SOLID, DASH, BLANK)
   * @param width Width of the line/Pen
   * @param c Color of the Pen
   */
  void set(enum style style, unsigned width, const Color c);
  /**
   * Sets the Pens parameters to the given values
   * @param width Width of the line/Pen
   * @param c Color of the Pen
   */
  void set(unsigned width, const Color c);
  /**
   * Resets the Pen to NULL
   */
  void reset();

  /**
   * Returns whether the Pen is defined (!= NULL)
   * @return True if the Pen is defined, False otherwise
   */
  bool
  defined() const
  {
    #ifdef ENABLE_SDL
    return width > 0;
    #else
    return pen != NULL;
    #endif
  }

  #ifdef ENABLE_SDL
  unsigned
  get_width() const
  {
    return width;
  }

  const Color
  get_color() const
  {
    return color;
  }
  #else
  /**
   * Returns the native HPEN object
   * @return The native HPEN object
   */
  HPEN native() const { return pen; }
  #endif
};

#endif
