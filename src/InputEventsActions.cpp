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

/*

InputEvents

This class is used to control all user and external InputEvents.
This includes some Nmea strings, virtual events (Glide Computer
Evnets) and Keyboard.

What it does not cover is Glide Computer normal processing - this
includes GPS and Vario processing.

What it does include is what to do when an automatic event (switch
to Climb mode) and user events are entered.

It also covers the configuration side of on screen labels.

For further information on config file formats see

source/Common/Data/Input/ALL
doc/html/advanced/input/ALL		http://xcsoar.sourceforge.net/advanced/input/

*/

#include "InputEvents.h"
#include "Protection.hpp"
#include "LogFile.hpp"
#include "Device/Parser.hpp"
#include "Device/device.hpp"
#include "Device/List.hpp"
#include "Device/Descriptor.hpp"
#include "SettingsComputer.hpp"
#include "SettingsMap.hpp"
#include "Math/FastMath.h"
#include "Dialogs.h"
#include "Message.hpp"
#include "Marks.hpp"
#include "InfoBoxes/InfoBoxLayout.hpp"
#include "InfoBoxes/InfoBoxManager.hpp"
#include "Units.hpp"
#include "MainWindow.hpp"
#include "Atmosphere.h"
#include "Gauge/GaugeFLARM.hpp"
#include "Profile/Profile.hpp"
#include "LocalPath.hpp"
#include "Profile/ProfileKeys.hpp"
#include "UtilsText.hpp"
#include "StringUtil.hpp"
#include "Audio/Sound.hpp"
#include "Interface.hpp"
#include "Components.hpp"
#include "Language.hpp"
#include "Logger/Logger.hpp"
#include "Asset.hpp"
#include "Logger/NMEALogger.hpp"
#include "Waypoint/Waypoints.hpp"
#include "Task/ProtectedTaskManager.hpp"
#include "Airspace/ProtectedAirspaceWarningManager.hpp"
#include "Engine/Airspace/Airspaces.hpp"
#include "Engine/Airspace/AirspaceAircraftPerformance.hpp"
#include "DrawThread.hpp"
#include "Replay/Replay.hpp"
#include "DeviceBlackboard.hpp"
#include "UtilsSettings.hpp"
#include "Pages.hpp"
#include "Hardware/AltairControl.hpp"

#include <assert.h>
#include <ctype.h>
#include <tchar.h>
#include <algorithm>

using std::min;
using std::max;

// -----------------------------------------------------------------------
// Execution - list of things you can do
// -----------------------------------------------------------------------


// TODO code: Keep marker text for use in log file etc.
void
InputEvents::eventMarkLocation(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("reset")) == 0) {
    marks->Reset();
  } else {
    marks->MarkLocation(Basic().Location, Basic().DateTime,
                        SettingsComputer().EnableSoundModes);
  }

  draw_thread->trigger_redraw();
}

void
InputEvents::eventSounds(const TCHAR *misc)
{
 // bool OldEnableSoundVario = EnableSoundVario;

  if (_tcscmp(misc, _T("toggle")) == 0)
    SetSettingsComputer().EnableSoundVario = !SettingsComputer().EnableSoundVario;
  else if (_tcscmp(misc, _T("on")) == 0)
    SetSettingsComputer().EnableSoundVario = true;
  else if (_tcscmp(misc, _T("off")) == 0)
    SetSettingsComputer().EnableSoundVario = false;
  else if (_tcscmp(misc, _T("show")) == 0) {
    if (SettingsComputer().EnableSoundVario)
      Message::AddMessage(_("Vario Sounds ON"));
    else
      Message::AddMessage(_("Vario Sounds OFF"));
  }
  /*
  if (EnableSoundVario != OldEnableSoundVario) {
    VarioSound_EnableSound(EnableSoundVario);
  }
  */
}

void
InputEvents::eventSnailTrail(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("toggle")) == 0) {
    SetSettingsMap().TrailActive = SettingsMap().TrailActive + 1;
    if (SettingsMap().TrailActive > 3) {
      SetSettingsMap().TrailActive = 0;
    }
  } else if (_tcscmp(misc, _T("off")) == 0)
    SetSettingsMap().TrailActive = 0;
  else if (_tcscmp(misc, _T("long")) == 0)
    SetSettingsMap().TrailActive = 1;
  else if (_tcscmp(misc, _T("short")) == 0)
    SetSettingsMap().TrailActive = 2;
  else if (_tcscmp(misc, _T("full")) == 0)
    SetSettingsMap().TrailActive = 3;
  else if (_tcscmp(misc, _T("show")) == 0) {
    if (SettingsMap().TrailActive == 0)
      Message::AddMessage(_("SnailTrail OFF"));
    if (SettingsMap().TrailActive == 1)
      Message::AddMessage(_("SnailTrail ON Long"));
    if (SettingsMap().TrailActive == 2)
      Message::AddMessage(_("SnailTrail ON Short"));
    if (SettingsMap().TrailActive == 3)
      Message::AddMessage(_("SnailTrail ON Full"));
  }
}

// VENTA3
/*
 * This even currently toggles DrawAirSpace() and does nothing else.
 * But since we use an int and not a bool, it is easy to expand it.
 * Note that XCSoar.cpp init OnAirSpace always to 1, and this value
 * is never saved to the registry actually. It is intended to be used
 * as a temporary choice during flight, does not affect configuration.
 * Note also that in MapWindow DrawAirSpace() is accomplished for
 * every OnAirSpace value >0 .  We can use negative numbers also,
 * but 0 should mean OFF all the way.
 */
void
InputEvents::eventAirSpace(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("toggle")) == 0) {
    SetSettingsMap().OnAirSpace++;

    if (SettingsMap().OnAirSpace > 1) {
      SetSettingsMap().OnAirSpace = 0;
    }
  } else if (_tcscmp(misc, _T("off")) == 0)
    SetSettingsMap().OnAirSpace = 0;
  else if (_tcscmp(misc, _T("on")) == 0)
    SetSettingsMap().OnAirSpace = 1;
  else if (_tcscmp(misc, _T("show")) == 0) {
    if (SetSettingsMap().OnAirSpace == 0)
      Message::AddMessage(_("Show Airspace OFF"));
    if (SetSettingsMap().OnAirSpace == 1)
      Message::AddMessage(_("Show Airspace ON"));
  }
}

void
InputEvents::eventScreenModes(const TCHAR *misc)
{
  // toggle switches like this:
  //  -- normal infobox
  //  -- auxiliary infobox
  //  -- full screen
  //  -- normal infobox

  using namespace Pages;

  if (_tcscmp(misc, _T("normal")) == 0) {
    PageLayout pl(PageLayout::t_Map);
    pl.MapInfoBoxes = PageLayout::mib_Normal;
    OpenLayout(pl);
  } else if (_tcscmp(misc, _T("auxilary")) == 0) {
    PageLayout pl(PageLayout::t_Map);
    pl.MapInfoBoxes = PageLayout::mib_Aux;
    OpenLayout(pl);
  } else if (_tcscmp(misc, _T("toggleauxiliary")) == 0) {
    PageLayout pl(PageLayout::t_Map);
    pl.MapInfoBoxes = SettingsMap().EnableAuxiliaryInfo ?
                      PageLayout::mib_Normal : PageLayout::mib_Aux;
    OpenLayout(pl);
  } else if (_tcscmp(misc, _T("full")) == 0) {
    PageLayout pl(PageLayout::t_Map);
    OpenLayout(pl);
  } else if (_tcscmp(misc, _T("togglefull")) == 0) {
    main_window.SetFullScreen(!main_window.GetFullScreen());
  } else if (_tcscmp(misc, _T("show")) == 0) {
    if (main_window.GetFullScreen())
      Message::AddMessage(_("Screen Mode Full"));
    else if (SettingsMap().EnableAuxiliaryInfo)
      Message::AddMessage(_("Screen Mode Auxiliary"));
    else
      Message::AddMessage(_("Screen Mode Normal"));
  } else if (_tcscmp(misc, _T("togglebiginfo")) == 0) {
    InfoBoxLayout::fullscreen = !InfoBoxLayout::fullscreen;
    InfoBoxManager::SetDirty();
  } else {
    Pages::Next();
  }
}

// eventAutoZoom - Turn on|off|toggle AutoZoom
// misc:
//	auto on - Turn on if not already
//	auto off - Turn off if not already
//	auto toggle - Toggle current full screen status
//	auto show - Shows autozoom status
//	+	- Zoom in
//	++	- Zoom in near
//	-	- Zoom out
//	--	- Zoom out far
//	n.n	- Zoom to a set scale
//	show - Show current zoom scale
void
InputEvents::eventZoom(const TCHAR* misc)
{
  // JMW pass through to handler in MapWindow
  // here:
  // -1 means toggle
  // 0 means off
  // 1 means on

  if (_tcscmp(misc, _T("auto toggle")) == 0)
    sub_AutoZoom(-1);
  else if (_tcscmp(misc, _T("auto on")) == 0)
    sub_AutoZoom(1);
  else if (_tcscmp(misc, _T("auto off")) == 0)
    sub_AutoZoom(0);
  else if (_tcscmp(misc, _T("auto show")) == 0) {
    if (SettingsMap().AutoZoom)
      Message::AddMessage(_("AutoZoom ON"));
    else
      Message::AddMessage(_("AutoZoom OFF"));
  } else if (_tcscmp(misc, _T("slowout")) == 0)
    sub_ScaleZoom(-4);
  else if (_tcscmp(misc, _T("slowin")) == 0)
    sub_ScaleZoom(4);
  else if (_tcscmp(misc, _T("out")) == 0)
    sub_ScaleZoom(-1);
  else if (_tcscmp(misc, _T("in")) == 0)
    sub_ScaleZoom(1);
  else if (_tcscmp(misc, _T("-")) == 0)
    sub_ScaleZoom(-1);
  else if (_tcscmp(misc, _T("+")) == 0)
    sub_ScaleZoom(1);
  else if (_tcscmp(misc, _T("--")) == 0)
    sub_ScaleZoom(-2);
  else if (_tcscmp(misc, _T("++")) == 0)
    sub_ScaleZoom(2);
  else if (_tcscmp(misc, _T("circlezoom toggle")) == 0) {
    SetSettingsMap().CircleZoom = !SettingsMap().CircleZoom;
  } else if (_tcscmp(misc, _T("circlezoom on")) == 0) {
    SetSettingsMap().CircleZoom = true;
  } else if (_tcscmp(misc, _T("circlezoom off")) == 0) {
    SetSettingsMap().CircleZoom = false;
  } else if (_tcscmp(misc, _T("circlezoom show")) == 0) {
    if (SettingsMap().CircleZoom)
      Message::AddMessage(_("Circling Zoom ON"));
    else
      Message::AddMessage(_("Circling Zoom OFF"));
  } else {
    TCHAR *endptr;
    double zoom = _tcstod(misc, &endptr);
    if (endptr == misc)
      return;

    sub_SetZoom(fixed(zoom));
  }

  SendSettingsMap(true);
}

/**
 * This function handles all "pan" input events
 * @param misc A string describing the desired pan action.
 *  on             Turn pan on
 *  off            Turn pan off
 *  toogle         Toogles pan mode
 *  supertoggle    Toggles pan mode and fullscreen
 *  up             Pan up
 *  down           Pan down
 *  left           Pan left
 *  right          Pan right
 *  @todo feature: n,n Go that direction - +/-
 *  @todo feature: ??? Go to particular point
 *  @todo feature: ??? Go to waypoint (eg: next, named)
 */
void
InputEvents::eventPan(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("toggle")) == 0)
    sub_Pan(-1);

  else if (_tcscmp(misc, _T("supertoggle")) == 0)
    sub_Pan(-2);

  else if (_tcscmp(misc, _T("on")) == 0)
    sub_Pan(1);

  else if (_tcscmp(misc, _T("off")) == 0)
    sub_Pan(0);

  else if (_tcscmp(misc, _T("up")) == 0)
    if (model_is_hp31x())
      // Scroll wheel on the HP31x series should zoom in pan mode
      sub_ScaleZoom(1);
    else
      sub_PanCursor(0, 1);

  else if (_tcscmp(misc, _T("down")) == 0)
    if (model_is_hp31x())
      // Scroll wheel on the HP31x series should zoom in pan mode
      sub_ScaleZoom(-1);
    else
      sub_PanCursor(0, -1);

  else if (_tcscmp(misc, _T("left")) == 0)
    sub_PanCursor(1, 0);

  else if (_tcscmp(misc, _T("right")) == 0)
    sub_PanCursor(-1, 0);

  else if (_tcscmp(misc, _T("show")) == 0) {
    if (SettingsMap().EnablePan)
      Message::AddMessage(_("Pan mode ON"));
    else
      Message::AddMessage(_("Pan mode OFF"));
  }

  SendSettingsMap(true);
}

// Do JUST Terrain/Toplogy (toggle any, on/off any, show)
void
InputEvents::eventTerrainTopology(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("terrain toggle")) == 0)
    sub_TerrainTopology(-2);
  else if (_tcscmp(misc, _T("topology toggle")) == 0)
    sub_TerrainTopology(-3);
  else if (_tcscmp(misc, _T("terrain on")) == 0)
    sub_TerrainTopology(3);
  else if (_tcscmp(misc, _T("terrain off")) == 0)
    sub_TerrainTopology(4);
  else if (_tcscmp(misc, _T("topology on")) == 0)
    sub_TerrainTopology(1);
  else if (_tcscmp(misc, _T("topology off")) == 0)
    sub_TerrainTopology(2);
  else if (_tcscmp(misc, _T("show")) == 0)
    sub_TerrainTopology(0);
  else if (_tcscmp(misc, _T("toggle")) == 0)
    sub_TerrainTopology(-1);

  SendSettingsMap(true);
}

// Do clear warnings IF NONE Toggle Terrain/Topology
void
InputEvents::eventClearWarningsOrTerrainTopology(const TCHAR *misc)
{
  (void)misc;

  if (!airspace_warnings.warning_empty()) {
    airspace_warnings.clear_warnings();
    return;
  }
  // Else toggle TerrainTopology - and show the results
  sub_TerrainTopology(-1);
  sub_TerrainTopology(0);
  SendSettingsMap(true);
}

// ClearAirspaceWarnings
// Clears airspace warnings for the selected airspace
void
InputEvents::eventClearAirspaceWarnings(const TCHAR *misc)
{
  airspace_warnings.clear_warnings();
}

// ClearStatusMessages
// Do Clear Event Warnings
void
InputEvents::eventClearStatusMessages(const TCHAR *misc)
{
  (void)misc;
  // TODO enhancement: allow selection of specific messages (here we are acknowledging all)
  main_window.popup.Acknowledge(0);
}

void
InputEvents::eventFLARMRadar(const TCHAR *misc)
{
  (void)misc;
  // if (_tcscmp(misc, _T("on")) == 0) {

  GaugeFLARM *gauge_flarm = main_window.flarm;
  if (gauge_flarm == NULL)
    return;

  if (_tcscmp(misc, _T("ForceToggle")) == 0) {
    gauge_flarm->ForceVisible = !gauge_flarm->ForceVisible;
    SetSettingsMap().EnableFLARMGauge = gauge_flarm->ForceVisible;
  } else
    gauge_flarm->Suppress = !gauge_flarm->Suppress;
  // the result of this will get triggered by refreshslots
}

void
InputEvents::eventThermalAssistant(const TCHAR *misc)
{
  (void)misc;
  dlgThermalAssistantShowModal();
}

// SelectInfoBox
// Selects the next or previous infobox
void
InputEvents::eventSelectInfoBox(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("next")) == 0)
    InfoBoxManager::Event_Select(1);
  else if (_tcscmp(misc, _T("previous")) == 0)
    InfoBoxManager::Event_Select(-1);
}

// ChangeInfoBoxType
// Changes the type of the current infobox to the next/previous type
void
InputEvents::eventChangeInfoBoxType(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("next")) == 0)
    InfoBoxManager::Event_Change(1);
  else if (_tcscmp(misc, _T("previous")) == 0)
    InfoBoxManager::Event_Change(-1);
}

// ArmAdvance
// Controls waypoint advance trigger:
//     on: Arms the advance trigger
//    off: Disarms the advance trigger
//   toggle: Toggles between armed and disarmed.
//   show: Shows current armed state
void
InputEvents::eventArmAdvance(const TCHAR *misc)
{
  ProtectedTaskManager::ExclusiveLease task_manager(protected_task_manager);
  const TaskAdvance::TaskAdvanceState_t mode =
    task_manager->get_task_advance().get_advance_state();
  
  if (_tcscmp(misc, _T("on")) == 0) {
    task_manager->get_task_advance().set_armed(true);
  } else if (_tcscmp(misc, _T("off")) == 0) {
    task_manager->get_task_advance().set_armed(false);
  } else if (_tcscmp(misc, _T("toggle")) == 0) {
    task_manager->get_task_advance().toggle_armed();
  } else if (_tcscmp(misc, _T("show")) == 0) {
    switch (mode) {
    case TaskAdvance::MANUAL:
      Message::AddMessage(_("Advance: Manual"));
      break;
    case TaskAdvance::AUTO:
      Message::AddMessage(_("Advance: Automatic"));
      break;
    case TaskAdvance::START_ARMED:
      Message::AddMessage(_("Advance: Ready to start"));
      break;
    case TaskAdvance::START_DISARMED:
      Message::AddMessage(_("Advance: Hold start"));
      break;
    case TaskAdvance::TURN_ARMED:
      Message::AddMessage(_("Advance: Ready to turn"));
      break;
    case TaskAdvance::TURN_DISARMED:
      Message::AddMessage(_("Advance: Hold turn"));
      break;
    }
  }
}

// DoInfoKey
// Performs functions associated with the selected infobox
//    up: triggers the up event
//    etc.
//    Functions associated with the infoboxes are described in the
//    infobox section in the reference guide
void InputEvents::eventDoInfoKey(const TCHAR *misc) {
  if (_tcscmp(misc, _T("up")) == 0)
    InfoBoxManager::ProcessKey(InfoBoxContent::ibkUp);
  else if (_tcscmp(misc, _T("down")) == 0)
    InfoBoxManager::ProcessKey(InfoBoxContent::ibkDown);
  else if (_tcscmp(misc, _T("left")) == 0)
    InfoBoxManager::ProcessKey(InfoBoxContent::ibkLeft);
  else if (_tcscmp(misc, _T("right")) == 0)
    InfoBoxManager::ProcessKey(InfoBoxContent::ibkRight);
  else if (_tcscmp(misc, _T("return")) == 0)
    InfoBoxManager::ProcessKey(InfoBoxContent::ibkEnter);
  else if (_tcscmp(misc, _T("setup")) == 0)
    InfoBoxManager::SetupFocused();
}

// Mode
// Sets the current event mode.
//  The argument is the label of the mode to activate.
//  This is used to activate menus/submenus of buttons
void
InputEvents::eventMode(const TCHAR *misc)
{
  assert(misc != NULL);

  main_window.map.set_focus();

  InputEvents::setMode(misc);
}

// Don't think we need this.
void
InputEvents::eventMainMenu(const TCHAR *misc)
{
  (void)misc;
  // todo: popup main menu
}

// Checklist
// Displays the checklist dialog
//  See the checklist dialog section of the reference manual for more info.
void
InputEvents::eventChecklist(const TCHAR *misc)
{
  (void)misc;
  dlgChecklistShowModal();
}

// FLARM Traffic
// Displays the FLARM traffic dialog
//  See the checklist dialog section of the reference manual for more info.
void
InputEvents::eventFlarmTraffic(const TCHAR *misc)
{
  (void)misc;
  dlgFlarmTrafficShowModal();
}

// Displays the task calculator dialog
//  See the task calculator dialog section of the reference manual
// for more info.
void
InputEvents::eventCalculator(const TCHAR *misc)
{
  (void)misc;
  dlgTaskCalculatorShowModal(main_window);
}

// Status
// Displays one of the three status dialogs:
//    system: display the system status
//    aircraft: displays the aircraft status
//    task: displays the task status
//  See the status dialog section of the reference manual for more info
//  on these.
void
InputEvents::eventStatus(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("system")) == 0) {
    dlgStatusShowModal(1);
  } else if (_tcscmp(misc, _T("task")) == 0) {
    dlgStatusShowModal(2);
  } else if (_tcscmp(misc, _T("aircraft")) == 0) {
    dlgStatusShowModal(0);
  } else {
    dlgStatusShowModal(-1);
  }
}

// Analysis
// Displays the analysis/statistics dialog
//  See the analysis dialog section of the reference manual
// for more info.
void
InputEvents::eventAnalysis(const TCHAR *misc)
{
  (void)misc;
  dlgAnalysisShowModal();
}

// WaypointDetails
// Displays waypoint details
//         current: the current active waypoint
//          select: brings up the waypoint selector, if the user then
//                  selects a waypoint, then the details dialog is shown.
//  See the waypoint dialog section of the reference manual
// for more info.
void
InputEvents::eventWaypointDetails(const TCHAR *misc)
{
  const Waypoint* wp = NULL;

  if (_tcscmp(misc, _T("current")) == 0) {

    wp = protected_task_manager.getActiveWaypoint();
    if (!wp) {
      Message::AddMessage(_("No Active Waypoint!"));
      return;
    }
  } else if (_tcscmp(misc, _T("select")) == 0) {
    wp = dlgWayPointSelect(main_window, Basic().Location);
  }
  if (wp) {
    dlgWayPointDetailsShowModal(main_window, *wp);
  }
}

void
InputEvents::eventGotoLookup(const TCHAR *misc)
{
  const Waypoint* wp = dlgWayPointSelect(main_window, Basic().Location);
  if (wp) {
    protected_task_manager.do_goto(*wp);
  }
}

// StatusMessage
// Displays a user defined status message.
//    The argument is the text to be displayed.
//    No punctuation characters are allowed.
void
InputEvents::eventStatusMessage(const TCHAR *misc)
{
  Message::AddMessage(misc);
}

// Plays a sound from the filename
void
InputEvents::eventPlaySound(const TCHAR *misc)
{
  PlayResource(misc);
}

// MacCready
// Adjusts MacCready settings
// up, down, auto on, auto off, auto toggle, auto show
void
InputEvents::eventMacCready(const TCHAR *misc)
{
  GlidePolar polar = protected_task_manager.get_glide_polar();
  fixed mc = polar.get_mc();

  if (_tcscmp(misc, _T("up")) == 0) {
    mc = std::min(mc + fixed_one / 10, fixed(5));
    polar.set_mc(mc);
    protected_task_manager.set_glide_polar(polar);
    device_blackboard.SetMC(mc);
  } else if (_tcscmp(misc, _T("down")) == 0) {
    mc = std::max(mc - fixed_one / 10, fixed_zero);
    polar.set_mc(mc);
    protected_task_manager.set_glide_polar(polar);
    device_blackboard.SetMC(mc);
  } else if (_tcscmp(misc, _T("auto toggle")) == 0) {
    SetSettingsComputer().auto_mc = !SettingsComputer().auto_mc;
  } else if (_tcscmp(misc, _T("auto on")) == 0) {
    SetSettingsComputer().auto_mc = true;
  } else if (_tcscmp(misc, _T("auto off")) == 0) {
    SetSettingsComputer().auto_mc = false;
  } else if (_tcscmp(misc, _T("auto show")) == 0) {
    if (SettingsComputer().auto_mc) {
      Message::AddMessage(_("Auto MacCready ON"));
    } else {
      Message::AddMessage(_("Auto MacCready OFF"));
    }
  } else if (_tcscmp(misc, _T("show")) == 0) {
    TCHAR Temp[100];
    Units::FormatUserVSpeed(mc,
                            Temp, sizeof(Temp) / sizeof(Temp[0]),
                            false);
    Message::AddMessage(_("MacCready "), Temp);
  }
}

// Wind
// Adjusts the wind magnitude and direction
//     up: increases wind magnitude
//   down: decreases wind magnitude
//   left: rotates wind direction counterclockwise
//  right: rotates wind direction clockwise
//   save: saves wind value, so it is used at next startup
//
// TODO feature: Increase wind by larger amounts ? Set wind to specific amount ?
//	(may sound silly - but future may get SMS event that then sets wind)
void
InputEvents::eventWind(const TCHAR *misc)
{
}

int jmw_demo=0;

// SendNMEA
//  Sends a user-defined NMEA string to an external instrument.
//   The string sent is prefixed with the start character '$'
//   and appended with the checksum e.g. '*40'.  The user needs only
//   to provide the text in between the '$' and '*'.
//
void
InputEvents::eventSendNMEA(const TCHAR *misc)
{
  if (misc)
    VarioWriteNMEA(misc);
}

void
InputEvents::eventSendNMEAPort1(const TCHAR *misc)
{
  const unsigned i = 0;

  if (misc != NULL && i < NUMDEV)
    devWriteNMEAString(DeviceList[i], misc);
}

void
InputEvents::eventSendNMEAPort2(const TCHAR *misc)
{
  const unsigned i = 1;

  if (misc != NULL && i < NUMDEV)
    devWriteNMEAString(DeviceList[i], misc);
}

// AdjustVarioFilter
// When connected to the Vega variometer, this adjusts
// the filter time constant
//     slow/medium/fast
// The following arguments can be used for diagnostics purposes
//     statistics:
//     diagnostics:
//     psraw:
//     switch:
// The following arguments can be used to trigger demo modes:
//     climbdemo:
//     stfdemo:
// Other arguments control vario setup:
//     save: saves the vario configuration to nonvolatile memory on the instrument
//     zero: Zero's the airspeed indicator's offset
//
void
InputEvents::eventAdjustVarioFilter(const TCHAR *misc)
{
  static int naccel = 0;
  if (_tcscmp(misc, _T("slow")) == 0)
    VarioWriteNMEA(_T("PDVSC,S,VarioTimeConstant,3"));
  else if (_tcscmp(misc, _T("medium")) == 0)
    VarioWriteNMEA(_T("PDVSC,S,VarioTimeConstant,2"));
  else if (_tcscmp(misc, _T("fast")) == 0)
    VarioWriteNMEA(_T("PDVSC,S,VarioTimeConstant,1"));
  else if (_tcscmp(misc, _T("statistics")) == 0) {
    VarioWriteNMEA(_T("PDVSC,S,Diagnostics,1"));
    jmw_demo = 0;
  } else if (_tcscmp(misc, _T("diagnostics")) == 0) {
    VarioWriteNMEA(_T("PDVSC,S,Diagnostics,2"));
    jmw_demo = 0;
  } else if (_tcscmp(misc, _T("psraw")) == 0)
    VarioWriteNMEA(_T("PDVSC,S,Diagnostics,3"));
  else if (_tcscmp(misc, _T("switch")) == 0)
    VarioWriteNMEA(_T("PDVSC,S,Diagnostics,4"));
  else if (_tcscmp(misc, _T("democlimb")) == 0) {
    VarioWriteNMEA(_T("PDVSC,S,DemoMode,0"));
    VarioWriteNMEA(_T("PDVSC,S,DemoMode,2"));
    jmw_demo = 2;
  } else if (_tcscmp(misc, _T("demostf"))==0) {
    VarioWriteNMEA(_T("PDVSC,S,DemoMode,0"));
    VarioWriteNMEA(_T("PDVSC,S,DemoMode,1"));
    jmw_demo = 1;
  } else if (_tcscmp(misc, _T("accel")) == 0) {
    switch (naccel) {
    case 0:
      VarioWriteNMEA(_T("PDVSC,R,AccelerometerSlopeX"));
      break;
    case 1:
      VarioWriteNMEA(_T("PDVSC,R,AccelerometerSlopeY"));
      break;
    case 2:
      VarioWriteNMEA(_T("PDVSC,R,AccelerometerOffsetX"));
      break;
    case 3:
      VarioWriteNMEA(_T("PDVSC,R,AccelerometerOffsetY"));
      break;
    default:
      naccel = 0;
      break;
    }
    naccel++;
    if (naccel > 3)
      naccel = 0;

  } else if (_tcscmp(misc, _T("xdemo")) == 0) {
    dlgVegaDemoShowModal();
  } else if (_tcscmp(misc, _T("zero"))==0) {
    // zero, no mixing
    if (!Basic().flight.Flying) {
      VarioWriteNMEA(_T("PDVSC,S,ZeroASI,1"));
    }
  } else if (_tcscmp(misc, _T("save")) == 0) {
    VarioWriteNMEA(_T("PDVSC,S,StoreToEeprom,2"));

  // accel calibration
  } else if (!Basic().flight.Flying) {
    if (_tcscmp(misc, _T("X1"))==0)
      VarioWriteNMEA(_T("PDVSC,S,CalibrateAccel,1"));
    else if (_tcscmp(misc, _T("X2"))==0)
      VarioWriteNMEA(_T("PDVSC,S,CalibrateAccel,2"));
    else if (_tcscmp(misc, _T("X3"))==0)
      VarioWriteNMEA(_T("PDVSC,S,CalibrateAccel,3"));
    else if (_tcscmp(misc, _T("X4"))==0)
      VarioWriteNMEA(_T("PDVSC,S,CalibrateAccel,4"));
    else if (_tcscmp(misc, _T("X5"))==0)
      VarioWriteNMEA(_T("PDVSC,S,CalibrateAccel,5"));
  }
}

// Adjust audio deadband of internal vario sounds
// +: increases deadband
// -: decreases deadband
void
InputEvents::eventAudioDeadband(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("+"))) {
    SetSettingsComputer().SoundDeadband++;
  }
  if (_tcscmp(misc, _T("-"))) {
    SetSettingsComputer().SoundDeadband--;
  }
  SetSettingsComputer().SoundDeadband =
      min(40, max(SettingsComputer().SoundDeadband,0));

  /*
  VarioSound_SetVdead(SoundDeadband);
  */

  Profile::SetSoundSettings(); // save to registry

  // TODO feature: send to vario if available
}

// AdjustWaypoint
// Adjusts the active waypoint of the task
//  next: selects the next waypoint, stops at final waypoint
//  previous: selects the previous waypoint, stops at start waypoint
//  nextwrap: selects the next waypoint, wrapping back to start after final
//  previouswrap: selects the previous waypoint, wrapping to final after start
void
InputEvents::eventAdjustWaypoint(const TCHAR *misc)
{
  ProtectedTaskManager::ExclusiveLease task_manager(protected_task_manager);
  if (_tcscmp(misc, _T("next")) == 0)
    task_manager->incrementActiveTaskPoint(1); // next
  else if (_tcscmp(misc, _T("nextwrap")) == 0)
    task_manager->incrementActiveTaskPoint(1); // next - with wrap
  else if (_tcscmp(misc, _T("previous")) == 0)
    task_manager->incrementActiveTaskPoint(-1); // previous
  else if (_tcscmp(misc, _T("previouswrap")) == 0)
    task_manager->incrementActiveTaskPoint(-1); // previous with wrap
}

// AbortTask
// Allows aborting and resuming of tasks
// abort: aborts the task if active
// resume: resumes the task if aborted
// toggle: toggles between abort and resume
// show: displays a status message showing the task abort status
void
InputEvents::eventAbortTask(const TCHAR *misc)
{
  ProtectedTaskManager::ExclusiveLease task_manager(protected_task_manager);

  if (_tcscmp(misc, _T("abort")) == 0)
    task_manager->abort();
  else if (_tcscmp(misc, _T("resume")) == 0)
    task_manager->resume();
  else if (_tcscmp(misc, _T("show")) == 0) {
    switch (task_manager->get_mode()) {
    case TaskManager::MODE_ABORT:
      Message::AddMessage(_("Task Aborted"));
      break;
    case TaskManager::MODE_GOTO:
      Message::AddMessage(_("Task Goto"));
      break;
    case TaskManager::MODE_ORDERED:
      Message::AddMessage(_("Task Ordered"));
      break;
    default:
      Message::AddMessage(_("No Task"));
    }
  } else {
    // toggle
    switch (task_manager->get_mode()) {
    case TaskManager::MODE_NULL:
    case TaskManager::MODE_ORDERED:
      task_manager->abort();
      break;
    case TaskManager::MODE_GOTO:
      if (task_manager->check_ordered_task()) {
        task_manager->resume();
      } else {
        task_manager->abort();
      }
      break;
    case TaskManager::MODE_ABORT:
      task_manager->resume();
      break;
    default:
      break;
    }
  }
}

// Bugs
// Adjusts the degradation of glider performance due to bugs
// up: increases the performance by 10%
// down: decreases the performance by 10%
// max: cleans the aircraft of bugs
// min: selects the worst performance (50%)
// show: shows the current bug degradation
void
InputEvents::eventBugs(const TCHAR *misc)
{
  ProtectedTaskManager::ExclusiveLease task_manager(protected_task_manager);
  GlidePolar polar = task_manager->get_glide_polar();
  fixed BUGS = polar.get_bugs();
  fixed oldBugs = BUGS;

  if (_tcscmp(misc, _T("up")) == 0) {
    BUGS += fixed_one / 10;
    if (BUGS > fixed_one)
      BUGS = fixed_one;
  } else if (_tcscmp(misc, _T("down")) == 0) {
    BUGS -= fixed_one / 10;
    if (BUGS < fixed_half)
      BUGS = fixed_half;
  } else if (_tcscmp(misc, _T("max")) == 0)
    BUGS = fixed_one;
  else if (_tcscmp(misc, _T("min")) == 0)
    BUGS = fixed_half;
  else if (_tcscmp(misc, _T("show")) == 0) {
    TCHAR Temp[100];
    _stprintf(Temp, _T("%d"), (int)(BUGS * 100));
    Message::AddMessage(_("Bugs Performance"), Temp);
  }

  if (BUGS != oldBugs) {
    polar.set_bugs(fixed(BUGS));
    task_manager->set_glide_polar(polar);
  }
}

// Ballast
// Adjusts the ballast setting of the glider
// up: increases ballast by 10%
// down: decreases ballast by 10%
// max: selects 100% ballast
// min: selects 0% ballast
// show: displays a status message indicating the ballast percentage
void
InputEvents::eventBallast(const TCHAR *misc)
{
  ProtectedTaskManager::ExclusiveLease task_manager(protected_task_manager);
  GlidePolar polar = task_manager->get_glide_polar();
  fixed BALLAST = polar.get_ballast();
  fixed oldBallast = BALLAST;

  if (_tcscmp(misc, _T("up")) == 0) {
    BALLAST += fixed_one / 10;
    if (BALLAST >= fixed_one)
      BALLAST = fixed_one;
  } else if (_tcscmp(misc, _T("down")) == 0) {
    BALLAST -= fixed_one / 10;
    if (BALLAST < fixed_zero)
      BALLAST = fixed_zero;
  } else if (_tcscmp(misc, _T("max")) == 0)
    BALLAST = fixed_one;
  else if (_tcscmp(misc, _T("min")) == 0)
    BALLAST = fixed_zero;
  else if (_tcscmp(misc, _T("show")) == 0) {
    TCHAR Temp[100];
    _stprintf(Temp, _T("%d"), (int)(BALLAST * 100));
    Message::AddMessage(_("Ballast %"), Temp);
  }

  if (BALLAST != oldBallast) {
    polar.set_ballast(fixed(BALLAST));
    task_manager->set_glide_polar(polar);
  }
}

void
InputEvents::eventAutoLogger(const TCHAR *misc)
{
  if (!SettingsComputer().DisableAutoLogger)
    eventLogger(misc);
}

// Logger
// Activates the internal IGC logger
//  start: starts the logger
// start ask: starts the logger after asking the user to confirm
// stop: stops the logger
// stop ask: stops the logger after asking the user to confirm
// toggle: toggles between on and off
// toggle ask: toggles between on and off, asking the user to confirm
// show: displays a status message indicating whether the logger is active
// nmea: turns on and off NMEA logging
// note: the text following the 'note' characters is added to the log file
void
InputEvents::eventLogger(const TCHAR *misc)
{
  // TODO feature: start logger without requiring feedback
  // start stop toggle addnote

  if (_tcscmp(misc, _T("start ask")) == 0)
    logger.guiStartLogger(Basic(),SettingsComputer());
  else if (_tcscmp(misc, _T("start")) == 0)
    logger.guiStartLogger(Basic(),SettingsComputer(),true);
  else if (_tcscmp(misc, _T("stop ask")) == 0)
    logger.guiStopLogger(Basic());
  else if (_tcscmp(misc, _T("stop")) == 0)
    logger.guiStopLogger(Basic(),true);
  else if (_tcscmp(misc, _T("toggle ask")) == 0)
    logger.guiToggleLogger(Basic(),SettingsComputer());
  else if (_tcscmp(misc, _T("toggle")) == 0)
    logger.guiToggleLogger(Basic(), SettingsComputer(),true);
  else if (_tcscmp(misc, _T("nmea")) == 0) {
    EnableLogNMEA = !EnableLogNMEA;
    if (EnableLogNMEA) {
      Message::AddMessage(_("NMEA Log ON"));
    } else {
      Message::AddMessage(_("NMEA Log OFF"));
    }
  } else if (_tcscmp(misc, _T("show")) == 0)
    if (logger.isLoggerActive()) {
      Message::AddMessage(_("Logger ON"));
    } else {
      Message::AddMessage(_("Logger OFF"));
    }
  else if (_tcsncmp(misc, _T("note"), 4) == 0)
    // add note to logger file if available..
    logger.LoggerNote(misc + 4);
}

// RepeatStatusMessage
// Repeats the last status message.  If pressed repeatedly, will
// repeat previous status messages
void
InputEvents::eventRepeatStatusMessage(const TCHAR *misc)
{
  (void)misc;
  // new interface
  // TODO enhancement: display only by type specified in misc field
  main_window.popup.Repeat(0);
}

// NearestAirspaceDetails
// Displays details of the nearest airspace to the aircraft in a
// status message.  This does nothing if there is no airspace within
// 100km of the aircraft.
// If the aircraft is within airspace, this displays the distance and bearing
// to the nearest exit to the airspace.

#include "Airspace/AirspaceVisibility.hpp"
#include "Airspace/AirspaceSoonestSort.hpp"

void 
InputEvents::eventNearestAirspaceDetails(const TCHAR *misc) 
{
  (void)misc;

  if (!dlgAirspaceWarningIsEmpty()) {
    airspaceWarningEvent.trigger();
    return;
  }

  const AIRCRAFT_STATE aircraft_state = ToAircraftState(Basic());
  AirspaceVisible visible(SettingsComputer(),
                          Basic().GetAltitudeBaroPreferred());
  AirspaceAircraftPerformanceSimple perf;
  AirspaceSoonestSort ans(aircraft_state, perf, fixed(1800), visible);

  const AbstractAirspace* as = ans.find_nearest(airspace_database);
  if (!as) {
    return;
  } 

  dlgAirspaceDetails(*as);

  // clear previous warning if any
  main_window.popup.Acknowledge(PopupMessage::MSG_AIRSPACE);

  // TODO code: No control via status data (ala DoStatusMEssage)
  // - can we change this?
//  Message::AddMessage(5000, Message::MSG_AIRSPACE, text);
}

// NearestWaypointDetails
// Displays the waypoint details dialog
//  aircraft: the waypoint nearest the aircraft
//  pan: the waypoint nearest to the pan cursor
void
InputEvents::eventNearestWaypointDetails(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("aircraft")) == 0)
    // big range..
    PopupNearestWaypointDetails(way_points, Basic().Location,
                                1.0e5);
  else if (_tcscmp(misc, _T("pan")) == 0)
    // big range..
    PopupNearestWaypointDetails(way_points,
                                XCSoarInterface::main_window.map.VisibleProjection().GetPanLocation(),
                                1.0e5);
}

// Null
// The null event does nothing.  This can be used to override
// default functionality
void
InputEvents::eventNull(const TCHAR *misc)
{
  (void)misc;
  // do nothing
}

// TaskLoad
// Loads the task of the specified filename
void
InputEvents::eventTaskLoad(const TCHAR *misc)
{
  TCHAR buffer[MAX_PATH];

  if (!string_is_empty(misc)) {
    LocalPath(buffer, misc);
    protected_task_manager.task_load(buffer);
  }
}

// TaskSave
// Saves the task to the specified filename
void
InputEvents::eventTaskSave(const TCHAR *misc)
{
  TCHAR buffer[MAX_PATH];

  if (!string_is_empty(misc)) {
    LocalPath(buffer, misc);
    protected_task_manager.task_save(buffer);
  }
}

// ProfileLoad
// Loads the profile of the specified filename
void
InputEvents::eventProfileLoad(const TCHAR *misc)
{
  if (!string_is_empty(misc)) {
    Profile::LoadFile(misc);

    WaypointFileChanged = true;
    TerrainFileChanged = true;
    TopologyFileChanged = true;
    AirspaceFileChanged = true;
    AirfieldFileChanged = true;
    PolarFileChanged = true;

    // assuming all is ok, we can...
    Profile::Use();
  }
}

// ProfileSave
// Saves the profile to the specified filename
void
InputEvents::eventProfileSave(const TCHAR *misc)
{
  if (!string_is_empty(misc))
    Profile::SaveFile(misc);
}

void
InputEvents::eventBeep(const TCHAR *misc)
{
  #ifndef DISABLEAUDIO
  MessageBeep(MB_ICONEXCLAMATION);
  #endif

  #if defined(GNAV)
  altair_control.ShortBeep();
  #endif
}

// Setup
// Activates configuration and setting dialogs
//  Basic: Basic settings (QNH/Bugs/Ballast/MaxTemperature)
//  Wind: Wind settings
//  Task: Task editor
//  Airspace: Airspace filter settings
//  Replay: IGC replay dialog
void
InputEvents::eventSetup(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("Basic")) == 0)
    dlgBasicSettingsShowModal();
  else if (_tcscmp(misc, _T("Wind")) == 0)
    dlgWindSettingsShowModal();
  else if (_tcscmp(misc, _T("System")) == 0)
    SystemConfiguration();
  else if (_tcscmp(misc, _T("Task")) == 0)
    dlgTaskManagerShowModal(main_window);
  else if (_tcscmp(misc, _T("Airspace")) == 0)
    dlgAirspaceShowModal(false);
  else if (_tcscmp(misc, _T("Weather")) == 0)
    dlgWeatherShowModal();
  else if (_tcscmp(misc, _T("Replay")) == 0) {
    if (!Basic().gps.MovementDetected || replay.NmeaReplayEnabled())
      dlgLoggerReplayShowModal();
  } else if (_tcscmp(misc, _T("Switches")) == 0)
    dlgSwitchesShowModal();
  else if (_tcscmp(misc, _T("Voice")) == 0)
    dlgVoiceShowModal();
  else if (_tcscmp(misc, _T("Teamcode")) == 0)
    dlgTeamCodeShowModal();
  else if (_tcscmp(misc, _T("Target")) == 0)
    dlgTargetShowModal();
}

// AdjustForecastTemperature
// Adjusts the maximum ground temperature used by the convection forecast
// +: increases temperature by one degree celsius
// -: decreases temperature by one degree celsius
// show: Shows a status message with the current forecast temperature
void
InputEvents::eventAdjustForecastTemperature(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("+")) == 0)
    CuSonde::adjustForecastTemperature(1.0);
  else if (_tcscmp(misc, _T("-")) == 0)
    CuSonde::adjustForecastTemperature(-1.0);
  else if (_tcscmp(misc, _T("show")) == 0) {
    TCHAR Temp[100];
    _stprintf(Temp, _T("%f"), CuSonde::maxGroundTemperature);
    Message::AddMessage(_("Forecast temperature"), Temp);
  }
}

// Run
// Runs an external program of the specified filename.
// Note that XCSoar will wait until this program exits.
void
InputEvents::eventRun(const TCHAR *misc)
{
  #ifdef WIN32
  PROCESS_INFORMATION pi;
  if (!::CreateProcess(misc, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi))
    return;

  // wait for program to finish!
  ::WaitForSingleObject(pi.hProcess, INFINITE);

  #else /* !WIN32 */
  system(misc);
  #endif /* !WIN32 */
}

void
InputEvents::eventDeclutterLabels(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("toggle")) == 0) {
    SetSettingsMap().DeclutterLabels++;
    SetSettingsMap().DeclutterLabels = SettingsMap().DeclutterLabels % 3;
  } else if (_tcscmp(misc, _T("on")) == 0)
    SetSettingsMap().DeclutterLabels = 2;
  else if (_tcscmp(misc, _T("off")) == 0)
    SetSettingsMap().DeclutterLabels = 0;
  else if (_tcscmp(misc, _T("mid")) == 0)
    SetSettingsMap().DeclutterLabels = 1;
  else if (_tcscmp(misc, _T("show")) == 0) {
    if (SettingsMap().DeclutterLabels == 0)
      Message::AddMessage(_("Map labels ON"));
    else if (SettingsMap().DeclutterLabels == 1)
      Message::AddMessage(_("Map labels MID"));
    else
      Message::AddMessage(_("Map labels OFF"));
  }
}

void
InputEvents::eventBrightness(const TCHAR *misc)
{
  (void)misc;

  dlgBrightnessShowModal();
}

void
InputEvents::eventExit(const TCHAR *misc)
{
  (void)misc;

  SignalShutdown(false);
}

void
InputEvents::eventUserDisplayModeForce(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("unforce")) == 0)
    SetSettingsMap().UserForceDisplayMode = dmNone;
  else if (_tcscmp(misc, _T("forceclimb")) == 0)
    SetSettingsMap().UserForceDisplayMode = dmCircling;
  else if (_tcscmp(misc, _T("forcecruise")) == 0)
    SetSettingsMap().UserForceDisplayMode = dmCruise;
  else if (_tcscmp(misc, _T("forcefinal")) == 0)
    SetSettingsMap().UserForceDisplayMode = dmFinalGlide;
  else if (_tcscmp(misc, _T("show")) == 0)
    Message::AddMessage(_("Map labels ON"));
}

void
InputEvents::eventAirspaceDisplayMode(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("all")) == 0)
    SetSettingsComputer().AltitudeMode = ALLON;
  else if (_tcscmp(misc, _T("clip")) == 0)
    SetSettingsComputer().AltitudeMode = CLIP;
  else if (_tcscmp(misc, _T("auto")) == 0)
    SetSettingsComputer().AltitudeMode = AUTO;
  else if (_tcscmp(misc, _T("below")) == 0)
    SetSettingsComputer().AltitudeMode = ALLBELOW;
  else if (_tcscmp(misc, _T("off")) == 0)
    SetSettingsComputer().AltitudeMode = ALLOFF;
}

void
InputEvents::eventAddWaypoint(const TCHAR *misc)
{
  Waypoint edit_waypoint = way_points.create(Basic().Location);
  edit_waypoint.Altitude = Basic().GPSAltitude;
  if (dlgWaypointEditShowModal(edit_waypoint)) {
    if (edit_waypoint.Name.size()) {
      ScopeSuspendAllThreads suspend;
      way_points.append(edit_waypoint);
    }
  }
}

void
InputEvents::eventOrientation(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("northup")) == 0)
    SetSettingsMap().DisplayOrientation = NORTHUP;
  else if (_tcscmp(misc, _T("northcircle")) == 0)
    SetSettingsMap().DisplayOrientation = NORTHCIRCLE;
  else if (_tcscmp(misc, _T("trackcircle")) == 0)
    SetSettingsMap().DisplayOrientation = TRACKCIRCLE;
  else if (_tcscmp(misc, _T("trackup")) == 0)
    SetSettingsMap().DisplayOrientation = TRACKUP;
  else if (_tcscmp(misc, _T("northtrack")) == 0)
    SetSettingsMap().DisplayOrientation = NORTHTRACK;
}

// JMW TODO enhancement: have all inputevents return bool, indicating whether
// the button should after processing be hilit or not.
// this allows the buttons to indicate whether things are enabled/disabled
// SDP TODO enhancement: maybe instead do conditional processing ?
//     I like this idea; if one returns false, then don't execute the
//     remaining events.

// JMW TODO enhancement: make sure when we change things here we also set registry values...
// or maybe have special tag "save" which indicates it should be saved (notice that
// the wind adjustment uses this already, see in Process.cpp)

/* Recently done

eventTaskLoad		- Load tasks from a file (misc = filename)
eventTaskSave		- Save tasks to a file (misc = filename)
eventProfileLoad		- Load profile from a file (misc = filename)
eventProfileSave		- Save profile to a file (misc = filename)

*/

/* TODO feature: - new events

eventPanWaypoint		                - Set pan to a waypoint
- Waypoint could be "next", "first", "last", "previous", or named
- Note: wrong name - probably just part of eventPan
eventPressure		- Increase, Decrease, show, Set pressure value
eventDeclare			- (JMW separate from internal logger)
eventAirspaceDisplay	- all, below nnn, below me, auto nnn
eventAirspaceWarnings- on, off, time nn, ack nn
eventTerrain			- see map_window.Event_Terrain
eventCompass			- on, off, cruise on, crusie off, climb on, climb off
eventVario			- on, off // JMW what does this do?
eventOrientation		- north, track,  ???
eventTerrainRange	        - on, off (might be part of eventTerrain)
eventSounds			- Include Task and Modes sounds along with Vario
- Include master nn, deadband nn, netto trigger mph/kts/...

*/

// helpers

/* Event_TerrainToplogy Changes
   0       Show
   1       Toplogy = ON
   2       Toplogy = OFF
   3       Terrain = ON
   4       Terrain = OFF
   -1      Toggle through 4 stages (off/off, off/on, on/off, on/on)
   -2      Toggle terrain
   -3      Toggle toplogy
 */

void
InputEvents::sub_TerrainTopology(int vswitch)
{
  if (vswitch == -1) {
    // toggle through 4 possible options
    char val = 0;

    if (SettingsMap().EnableTopology)
      val++;
    if (SettingsMap().EnableTerrain)
      val += (char)2;

    val++;
    if (val > 3)
      val = 0;

    SetSettingsMap().EnableTopology = ((val & 0x01) == 0x01);
    SetSettingsMap().EnableTerrain = ((val & 0x02) == 0x02);
  } else if (vswitch == -2)
    // toggle terrain
    SetSettingsMap().EnableTerrain = !SettingsMap().EnableTerrain;
  else if (vswitch == -3)
    // toggle topology
    SetSettingsMap().EnableTopology = !SettingsMap().EnableTopology;
  else if (vswitch == 1)
    // Turn on topology
    SetSettingsMap().EnableTopology = true;
  else if (vswitch == 2)
    // Turn off topology
    SetSettingsMap().EnableTopology = false;
  else if (vswitch == 3)
    // Turn on terrain
    SetSettingsMap().EnableTerrain = true;
  else if (vswitch == 4)
    // Turn off terrain
    SetSettingsMap().EnableTerrain = false;
  else if (vswitch == 0) {
    // Show terrain/Topology
    // ARH Let user know what's happening
    TCHAR buf[128];

    if (SettingsMap().EnableTopology)
      _stprintf(buf, _T("\r\n%s / "), _("ON"));
    else
      _stprintf(buf, _T("\r\n%s / "), _("OFF"));

    _tcscat(buf, SettingsMap().EnableTerrain
            ? _("ON") : _("OFF"));

    Message::AddMessage(_("Topology / Terrain"), buf);
  }
}

/**
 * This function switches the pan mode on and off
 * @param vswitch This parameter determines what to do:
 * -2 supertoogle
 * -1 toogle
 * 1  on
 * 0  off
 * @see InputEvents::eventPan()
 */
void
InputEvents::sub_Pan(int vswitch)
{
  bool oldPan = SettingsMap().EnablePan;

  if (vswitch == -2) {
    // supertoogle, toogle pan mode and fullscreen
    SetSettingsMap().EnablePan = !SettingsMap().EnablePan;
  } else if (vswitch == -1)
    // toogle, toogle pan mode only
    SetSettingsMap().EnablePan = !SettingsMap().EnablePan;
  else
    // 1 = enable pan mode
    // 0 = disable pan mode
    SetSettingsMap().EnablePan = (vswitch !=0);

  if (SettingsMap().EnablePan != oldPan) {
    if (SettingsMap().EnablePan) {
      SetSettingsMap().PanLocation = Basic().Location;
      setMode(MODE_PAN);
    } else {
      setMode(MODE_DEFAULT);
    }
  }
}

void
InputEvents::sub_PanCursor(int dx, int dy)
{
  const Projection &projection = main_window.map.VisibleProjection();

  const RECT &MapRect = projection.GetMapRect();
  int X = (MapRect.right + MapRect.left) / 2;
  int Y = (MapRect.bottom + MapRect.top) / 2;

  const GeoPoint pstart = projection.Screen2LonLat(X, Y);

  X += (MapRect.right - MapRect.left) * dx / 4;
  Y += (MapRect.bottom - MapRect.top) * dy / 4;
  const GeoPoint pnew = projection.Screen2LonLat(X, Y);

  if (SettingsMap().EnablePan) {
    SetSettingsMap().PanLocation.Longitude += pstart.Longitude - pnew.Longitude;
    SetSettingsMap().PanLocation.Latitude += pstart.Latitude - pnew.Latitude;
  }

  main_window.map.QuickRedraw(SettingsMap());
  SendSettingsMap(true);
}

// called from UI or input event handler (same thread)
void
InputEvents::sub_AutoZoom(int vswitch)
{
  if (vswitch == -1)
    SetSettingsMap().AutoZoom = !SettingsMap().AutoZoom;
  else
    SetSettingsMap().AutoZoom = (vswitch != 0); // 0 off, 1 on

  if (SettingsMap().AutoZoom && SettingsMap().EnablePan)
    SetSettingsMap().EnablePan = false;
}

void
InputEvents::sub_SetZoom(fixed value)
{
  if (SetSettingsMap().AutoZoom) {
    SetSettingsMap().AutoZoom = false;  // disable autozoom if user manually changes zoom
    Message::AddMessage(_("AutoZoom OFF"));
  }
  SetSettingsMap().MapScale = value;

  main_window.map.QuickRedraw(SettingsMap());
  SendSettingsMap(true);
}

void
InputEvents::sub_ScaleZoom(int vswitch)
{
  const MapWindowProjection &projection = main_window.map.VisibleProjection();

  fixed value;
  if (positive(SettingsMap().MapScale))
    value = SettingsMap().MapScale;
  else
    value = projection.GetMapScaleUser();

  if (projection.HaveScaleList()) {
    value = projection.StepMapScale(fixed(value), -vswitch);
  } else {
    if (abs(vswitch) >= 4) {
      if (vswitch == 4)
        vswitch = 1;
      else if (vswitch == -4)
        vswitch = -1;
    }

    if (vswitch == 1)
      // zoom in a little
      value /= fixed_sqrt_two;
    else if (vswitch == -1)
      // zoom out a little
      value *= fixed_sqrt_two;
    else if (vswitch == 2)
      // zoom in a lot
      value /= 2;
    else if (vswitch == -2)
      // zoom out a lot
      value *= 2;
  }

  sub_SetZoom(value);
}

#include "LocalTime.hpp"


void
InputEvents::eventTaskTransition(const TCHAR *misc)
{
  if (_tcscmp(misc, _T("start")) == 0) {
    AIRCRAFT_STATE start_state = protected_task_manager.get_start_state();

    TCHAR TempTime[40];
    TCHAR TempAlt[40];
    TCHAR TempSpeed[40];
    
    Units::TimeToText(TempTime, (int)TimeLocal((int)start_state.Time));
    Units::FormatUserAltitude(start_state.NavAltitude,
                              TempAlt, sizeof(TempAlt)/sizeof(TCHAR), true);
    Units::FormatUserSpeed(start_state.Speed,
                           TempSpeed, sizeof(TempSpeed)/sizeof(TCHAR), true);
    
    TCHAR TempAll[120];
    _stprintf(TempAll, _T("\r\nAltitude: %s\r\nSpeed:%s\r\nTime: %s"),
              TempAlt, TempSpeed, TempTime);
    Message::AddMessage(_("Task Start"), TempAll);
  } else if (_tcscmp(misc, _T("tp")) == 0) {
    Message::AddMessage(_("Next turnpoint"));
  } else if (_tcscmp(misc, _T("finish")) == 0) {
    Message::AddMessage(_("Task Finish"));
  } else if (_tcscmp(misc, _T("ready")) == 0) {
    Message::AddMessage(_("In sector, arm advance when ready"));
  }
}
