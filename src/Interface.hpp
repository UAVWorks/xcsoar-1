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

#if !defined(XCSOAR_INTERFACE_HPP)
#define XCSOAR_INTERFACE_HPP

#include "InterfaceBlackboard.hpp"
#include "Compiler.h"

class MainWindow;
class StatusMessageList;

/** 
 * Class to hold data/methods accessible by all interface subsystems
 */
class CommonInterface {
public:
  /** Instance of the main application */
  static HINSTANCE hInst;

  // window.. make this protected TODO so have to subclass to get access
  static StatusMessageList status_messages;
  static MainWindow main_window;

  // TODO: make this protected
  /**
   * Returns InterfaceBlackboard.Basic (NMEA_INFO) (read-only)
   * @return InterfaceBlackboard.Basic
   */
  gcc_const
  static const NMEA_INFO& Basic() { return blackboard.Basic(); }

  /**
   * Returns InterfaceBlackboard.Calculated (DERIVED_INFO) (read-only)
   * @return InterfaceBlackboard.Calculated
   */
  gcc_const
  static const DERIVED_INFO& Calculated() { return blackboard.Calculated(); }

  /**
   * Returns InterfaceBlackboard.MapProjection (MapWindowProjection) (read-only)
   * @return InterfaceBlackboard.MapProjection
   */
  gcc_const
  static const MapWindowProjection& MapProjection()
  { return blackboard.MapProjection(); }

  /**
   * Returns the InterfaceBlackboard.SettingsComputer (read-only)
   * @return The InterfaceBlackboard.SettingsComputer
   */
  gcc_const
  static const SETTINGS_COMPUTER& SettingsComputer()
  { return blackboard.SettingsComputer(); }

  /**
   * Returns the InterfaceBlackboard.SettingsComputer (read-write)
   * @return The InterfaceBlackboard.SettingsComputer
   */
  gcc_const
  static SETTINGS_COMPUTER& SetSettingsComputer()
  { return blackboard.SetSettingsComputer(); }

  /**
   * Returns the InterfaceBlackboard.SettingsMap (read-only)
   * @return The InterfaceBlackboard.SettingsMap
   */
  gcc_const
  static const SETTINGS_MAP& SettingsMap()
  { return blackboard.SettingsMap(); }

  /**
   * Returns the InterfaceBlackboard.SettingsMap (read-write)
   * @return The InterfaceBlackboard.SettingsMap
   */
  gcc_const
  static SETTINGS_MAP& SetSettingsMap()
  { return blackboard.SetSettingsMap(); }

  static void ReadMapProjection(const MapWindowProjection &map) {
    blackboard.ReadMapProjection(map);
  }
  static void ReadBlackboardBasic(const NMEA_INFO& nmea_info) {
    blackboard.ReadBlackboardBasic(nmea_info);
  }
  static void ReadBlackboardCalculated(const DERIVED_INFO& derived_info) {
    blackboard.ReadBlackboardCalculated(derived_info);
  }

private:
  static InterfaceBlackboard blackboard;

public:
  // settings
  static bool EnableAutoBacklight;
};

/** 
 * Class to hold data/methods accessible by interface subsystems
 * that can perform actions
 */
class ActionInterface: public CommonInterface {
public:
  // settings
  static unsigned MenuTimeoutMax;

protected:
  static void DisplayModes();
  static void SendSettingsComputer();

public:
  static void SendSettingsMap(const bool trigger_draw = false);

public:
  // ideally these should be protected
  static void SignalShutdown(bool force);
  static bool LockSettingsInFlight;
  static unsigned UserLevel;
};

/** 
 * Class to hold data/methods accessible by interface subsystems
 * of main program
 */
class XCSoarInterface: public ActionInterface {
public:
  // settings
  static unsigned debounceTimeout;

public:
  static bool Debounce();

  static bool InterfaceTimeoutZero();
  static void InterfaceTimeoutReset();
  static bool InterfaceTimeoutCheck();
  static bool CheckShutdown();

  static void AfterStartup();
  static void Shutdown();
  static bool Startup(HINSTANCE);

  static void ExchangeBlackboard();
  static void ReceiveMapProjection();
  static void ReceiveBlackboard();

private:
  static bool LoadProfile();
};

#endif
