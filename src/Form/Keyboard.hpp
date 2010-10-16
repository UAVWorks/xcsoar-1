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

#ifndef XCSOAR_KEYBOARD_CONTROL_HPP
#define XCSOAR_KEYBOARD_CONTROL_HPP

#include "Screen/ContainerWindow.hpp"
#include "Screen/ButtonWindow.hpp"

#include <tchar.h>

/**
 * The PanelControl class implements the simplest form of a ContainerControl
 */
class KeyboardControl : public ContainerWindow {
public:
  typedef void (*OnCharacterCallback_t)(TCHAR key);

protected:
  enum {
    MAX_BUTTONS = 40,
  };

  unsigned num_buttons;
  ButtonWindow buttons[MAX_BUTTONS];
  TCHAR button_values[MAX_BUTTONS];

public:
  /**
   * Constructor of the KeyboardControl class
   * @param parent the parent window
   * @param x x-Coordinate of the Control
   * @param y y-Coordinate of the Control
   * @param width Width of the Control
   * @param height Height of the Control
   */
  KeyboardControl(ContainerWindow &parent, int x, int y,
                  unsigned width, unsigned height,
                  Color background_color,
                  OnCharacterCallback_t function,
                  const WindowStyle _style = WindowStyle());

  /**
   * Show only the buttons representing the specified character list.
   */
  void SetAllowedCharacters(const TCHAR *allowed);

  void SetOnCharacterCallback(OnCharacterCallback_t Function) {
    mOnCharacter = Function;
  }

protected:
  virtual void on_paint(Canvas &canvas);
  virtual bool on_command(unsigned id, unsigned code);
  virtual bool on_resize(unsigned width, unsigned height);

private:
  Color background_color;

  unsigned button_width;
  unsigned button_height;

  ButtonWindow *get_button(TCHAR ch);

  void move_button(TCHAR ch, int left, int top);
  void resize_button(TCHAR ch, unsigned int width, unsigned int height);
  void resize_buttons();
  void set_buttons_size();
  void move_buttons_to_row(const TCHAR* buttons, int row, int offset_left = 0);
  void move_buttons();

  bool is_landscape();

  void add_button(const TCHAR* caption);

  OnCharacterCallback_t mOnCharacter;
};

#endif
