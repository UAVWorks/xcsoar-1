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

#ifndef XCSOAR_MAIN_WINDOW_HXX
#define XCSOAR_MAIN_WINDOW_HXX

#include "Screen/SingleWindow.hpp"
#include "GlueMapWindow.hpp"
#include "PopupMessage.hpp"

class GaugeVario;
class GaugeFLARM;
class GaugeThermalAssistant;
class StatusMessageList;

/**
 * The XCSoar main window.
 */
class MainWindow : public SingleWindow {
public:
  GlueMapWindow map;
  GaugeVario *vario;
  GaugeFLARM *flarm;
  GaugeThermalAssistant *ta;
  PopupMessage popup;

private:
  timer_t timer_id;

  RECT map_rect;
  bool FullScreen;

public:
  MainWindow(const StatusMessageList &status_messages)
    :vario(NULL), flarm(NULL), ta(NULL), popup(status_messages, *this),
     FullScreen(false) {}
  virtual ~MainWindow();

  static bool find(const TCHAR *text) {
    return TopWindow::find(_T("XCSoarMain"), text);
  }

  static bool register_class(HINSTANCE hInstance);

  void set(const TCHAR *text,
           int left, int top, unsigned width, unsigned height);

  void Initialise();
  void InitialiseConfigured();

  void reset() {
    map.reset();
    TopWindow::reset();
  }

  bool GetFullScreen() const {
    return FullScreen;
  }

  void SetFullScreen(bool _full_screen);

protected:
  bool on_activate();
  bool on_setfocus();
  bool on_timer(timer_t id);
  bool on_create();
  bool on_destroy();
  bool on_close();
};

#endif
