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

#ifndef XCSOAR_FORM_FORM_HPP
#define XCSOAR_FORM_FORM_HPP

#include "Screen/ContainerWindow.hpp"
#include "Dialogs/Dialogs.h"
#include "Util/tstring.hpp"

#include <map>
#include <list>

class SingleWindow;
class PeriodClock;

/**
 * A WndForm represents a Window with a titlebar.
 * It is used to display the XML dialogs and MessageBoxes.
 */
class WndForm: public ContainerWindow
{
  struct tstring_less_than {
    bool operator()(const tstring &a, const tstring &b) const
    {
      return a.compare(b) < 0;
    }
  };

  typedef std::list<Window *> window_list_t;
  typedef std::map<tstring, Window *, tstring_less_than> name_to_window_t;

  class ClientAreaWindow : public ContainerWindow {
  public:
    typedef bool (*CommandCallback_t)(unsigned cmd);
    CommandCallback_t mCommandCallback;

  protected:
    Color background_color;
    Brush background_brush;

  public:
    ClientAreaWindow():mCommandCallback(NULL) {}

    Color GetBackColor() const {
      return background_color;
    }

    void SetBackColor(Color color) {
      background_color = color;
      background_brush.set(color);
    }

  protected:
    virtual bool on_command(unsigned id, unsigned code);
    virtual Brush *on_color(Window &window, Canvas &canvas);
    virtual void on_paint(Canvas &canvas);
  };

public:
  typedef void (*TimerNotifyCallback_t)(WndForm &Sender);
  typedef bool (*KeyDownNotifyCallback_t)(WndForm &Sender, unsigned key_code);

private:
  /**
   * List of windows which will be deleted by the destructor of this
   * class.
   */
  window_list_t destruct_windows;

  /**
   * Mapping of control names to #Window objects.
   */
  name_to_window_t name_to_window;

  /**
   * List of windows which should only be visible in "advanced" mode.
   */
  window_list_t advanced_windows;

protected:
  SingleWindow &main_window;
  int mModalResult;
  /** Background color of the titlebar */
  Color mColorTitle;
  /** Font of the titlebar */
  const Font *mhTitleFont;
  /** The ClientWindow */
  ClientAreaWindow client_area;
  /** Coordinates of the ClientWindow */
  RECT mClientRect;
  /** Coordinates of the titlebar */
  RECT mTitleRect;

  TimerNotifyCallback_t mOnTimerNotify;
  KeyDownNotifyCallback_t mOnKeyDownNotify;

  /**
   * The on_paint event is called when the button needs to be drawn
   * (derived from PaintWindow)
   */
  virtual void on_paint(Canvas &canvas);

  timer_t cbTimerID;

  TCHAR mCaption[256];

public:
  /**
   * Constructor of the WndForm class
   * @param _main_window
   * @param Caption Titlebar text of the Window
   * @param X x-Coordinate of the Window
   * @param Y y-Coordinate of the Window
   * @param Width Width of the Window
   * @param Height Height of the Window
   */
  WndForm(SingleWindow &_main_window, int X, int Y, int Width, int Height,
          const TCHAR *Caption = _T(""),
          const WindowStyle style = WindowStyle());

  /** Destructor */
  virtual ~WndForm();

protected:
  void UpdateLayout();

public:
  ContainerWindow &GetClientAreaWindow(void);

  unsigned GetTitleHeight() const {
    return mTitleRect.bottom - mTitleRect.top;
  }

  /**
   * Add a #Window to the "destruct" list: the object will be deleted
   * by the destructor of this class.  This means that the caller
   * doesn't have to keep track of the specified Window, because this
   * WndForm is now responsible for freeing memory.
   */
  void AddDestruct(Window *window) {
    destruct_windows.push_front(window);
  }

  /**
   * Adds a #Window to the name-to-window map.
   */
  void AddNamed(const TCHAR *name, Window *window) {
    name_to_window[name] = window;
  }

  /**
   * Finds the ancestor window with the specified name.
   *
   * @param name the name of the #Window that is searched
   * @return the Window, or NULL if not found
   */
  Window *FindByName(const TCHAR *name);

  /**
   * Finds the ancestor window with the specified name.
   *
   * @param name the name of the #Window that is searched
   * @return the Window, or NULL if not found
   */
  const Window *FindByName(const TCHAR *name) const {
    return const_cast<WndForm *>(this)->FindByName(name);
  }

  /**
   * Adds a #Window to the "advanced window list" (#advanced_windows).
   */
  void AddAdvanced(Window *window) {
    advanced_windows.push_back(window);
  }

  /**
   * Shows/Hides the ClientControls depending on the given value of advanced and
   * whether their caption includes an asterisk.
   * @param advanced True if advanced mode activated
   */
  void FilterAdvanced(bool advanced);

  int GetModalResult(void) { return mModalResult; }
  int SetModalResult(int Value) {
    mModalResult = Value;
    return Value;
  }

  /** Set the font of the titlebar */
  void SetTitleFont(const Font &font);

  int ShowModal(bool bEnableMap = false);

  const TCHAR *GetCaption() const {
    return mCaption;
  }

  /** Set the titlebar text */
  void SetCaption(const TCHAR *Value);

  /** from class Window */
  virtual bool on_resize(unsigned width, unsigned height);
  virtual bool on_destroy();
  virtual bool on_timer(timer_t id);
  virtual bool on_command(unsigned id, unsigned code);

  Color GetBackColor() const {
    return client_area.GetBackColor();
  }

  /** Set the background color of the window */
  void SetBackColor(Color Value);

  void SetKeyDownNotify(KeyDownNotifyCallback_t KeyDownNotify) {
    mOnKeyDownNotify = KeyDownNotify;
  }

  void SetTimerNotify(TimerNotifyCallback_t OnTimerNotify);

  void SetCommandCallback(ClientAreaWindow::CommandCallback_t CommandCallback) {
    client_area.mCommandCallback = CommandCallback;
  }

private:
  static PeriodClock timeAnyOpenClose; // when any dlg opens or child closes
};

#endif
