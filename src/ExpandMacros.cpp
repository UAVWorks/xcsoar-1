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

#include "ButtonLabel.hpp"
#include "Language.hpp"
#include "Gauge/GaugeFLARM.hpp"
#include "Logger/Logger.hpp"
#include "MainWindow.hpp"
#include "Interface.hpp"
#include "SettingsComputer.hpp"
#include "Components.hpp"
#include "Compatibility/string.h"
#include "SettingsMap.hpp"
#include "Simulator.hpp"
#include "Waypoint/Waypoints.hpp"
#include "Engine/Airspace/Airspaces.hpp"
#include "Task/ProtectedTaskManager.hpp"
#include "Replay/Replay.hpp"

#include <stdlib.h>

/**
 * Replaces ToReplace with ReplaceWith in String
 * @param String Buffer string
 * @param ToReplace The string that will be replaced
 * @param ReplaceWith The replacement
 * @param Size (?)
 */
static void
ReplaceInString(TCHAR *String, const TCHAR *ToReplace,
                const TCHAR *ReplaceWith, size_t Size)
{
  TCHAR TmpBuf[MAX_PATH];
  size_t iR = _tcslen(ToReplace);
  TCHAR *pC;

  while ((pC = _tcsstr(String, ToReplace)) != NULL) {
    _tcscpy(TmpBuf, pC + iR);
    _tcscpy(pC, ReplaceWith);
    _tcscat(pC, TmpBuf);
  }
}

/**
 * If Condition is true, Macro in Buffer will be replaced by TrueText,
 * otherwise by FalseText.
 * @param Condition Condition to be checked
 * @param Buffer Buffer string
 * @param Macro The string that will be replaced
 * @param TrueText The replacement if Condition is true
 * @param FalseText The replacement if Condition is false
 * @param Size (?)
 */
static void
CondReplaceInString(bool Condition, TCHAR *Buffer, const TCHAR *Macro,
                    const TCHAR *TrueText, const TCHAR *FalseText, size_t Size)
{
  if (Condition)
    ReplaceInString(Buffer, Macro, TrueText, Size);
  else
    ReplaceInString(Buffer, Macro, FalseText, Size);
}

static bool
ExpandTaskMacros(TCHAR *OutBuffer, size_t Size,
                 const DERIVED_INFO &calculated,
                 const SETTINGS_COMPUTER &settings_computer)
{
  bool invalid = false;
  ProtectedTaskManager::Lease task_manager(protected_task_manager);

  if (_tcsstr(OutBuffer, _T("$(CheckTaskResumed)"))) {
    // TODO code: check, does this need to be set with temporary task?
    if (task_manager->is_mode(TaskManager::MODE_ABORT) ||
        task_manager->is_mode(TaskManager::MODE_GOTO))
      invalid = true;
    ReplaceInString(OutBuffer, _T("$(CheckTaskResumed)"), _T(""), Size);
  }

  if (_tcsstr(OutBuffer, _T("$(CheckTask)"))) {
    if (!calculated.task_stats.task_valid)
      invalid = true;

    ReplaceInString(OutBuffer, _T("$(CheckTask)"), _T(""), Size);
  }

  const AbstractTask *task = task_manager->get_active_task();
  if (task == NULL || !task->get_stats().task_valid ||
      task_manager->is_mode(TaskManager::MODE_GOTO)) {

    if (_tcsstr(OutBuffer, _T("$(WaypointNext)"))) {
      invalid = true;
      ReplaceInString(OutBuffer, _T("$(WaypointNext)"),
          _("Next turnpoint"), Size);

    } else if (_tcsstr(OutBuffer, _T("$(WaypointPrevious)"))) {
      invalid = true;
      ReplaceInString(OutBuffer, _T("$(WaypointPrevious)"),
          _("Previous turnpoint"), Size);
    }

  } else if (task_manager->is_mode(TaskManager::MODE_ABORT)) {

    if (_tcsstr(OutBuffer, _T("$(WaypointNext)"))) {
      if (!task->validTaskPoint(1))
        invalid = true;

      CondReplaceInString(task->validTaskPoint(1) && !task->validTaskPoint(2),
                          OutBuffer,
                          _T("$(WaypointNext)"),
                          _("Furthest landpoint"),
                          _("Next landpoint"), Size);

    } else if (_tcsstr(OutBuffer, _T("$(WaypointPrevious)"))) {
      if (!task->validTaskPoint(-1))
        invalid = true;

      CondReplaceInString(task->validTaskPoint(-1) && !task->validTaskPoint(-2),
                          OutBuffer,
                          _T("$(WaypointPrevious)"),
                          _("Closest landpoint"),
                          _("Previous landpoint"), Size);
    }

  } else {
    if (_tcsstr(OutBuffer, _T("$(WaypointNext)"))) {
      // Waypoint\nNext
      if (!task->validTaskPoint(1))
        invalid = true;

      CondReplaceInString(task->validTaskPoint(1) && !task->validTaskPoint(2),
                          OutBuffer,
                          _T("$(WaypointNext)"),
                          _("Finish turnpoint"),
                          _("Next turnpoint"), Size);

    } else if (_tcsstr(OutBuffer, _T("$(WaypointPrevious)"))) {
      if (!task->validTaskPoint(-1))
        invalid = true;

      CondReplaceInString(task->validTaskPoint(-1) && !task->validTaskPoint(-2),
                          OutBuffer,
                          _T("$(WaypointPrevious)"),
                          _("Start turnpoint"),
                          _("Previous turnpoint"), Size);
    } 
#ifdef OLD_TASK // multiple start points
    else if (task.getSettings().EnableMultipleStartPoints) {
      invalid |= !task.ValidTaskPoint(0);
      CondReplaceInString((task.getActiveIndex()==0),
                          OutBuffer,
                          _T("$(WaypointPrevious)"),
                          _("Start point cycle"), _("Previous waypoint"),
                          Size);
    } 
    else {
      invalid |= !calculated.common_stats.active_has_previous;
      ReplaceInString(OutBuffer, _T("$(WaypointPrevious)"),
                      _("Previous waypoint"), Size);
    }
#endif
  }

  if (_tcsstr(OutBuffer, _T("$(AdvanceArmed)"))) {
    switch (task_manager->get_task_advance().get_advance_state()) {
    case TaskAdvance::MANUAL:
      ReplaceInString(OutBuffer, _T("$(AdvanceArmed)"), 
                      _("Advance\n(manual)"), Size);
      invalid = true;
      break;
    case TaskAdvance::AUTO:
      ReplaceInString(OutBuffer, _T("$(AdvanceArmed)"), 
                      _("Advance\n(auto)"), Size);
      invalid = true;
      break;
    case TaskAdvance::START_ARMED:
      ReplaceInString(OutBuffer, _T("$(AdvanceArmed)"), 
                      _("Abort\nStart"), Size);
      invalid = false;
      break;
    case TaskAdvance::START_DISARMED:
      ReplaceInString(OutBuffer, _T("$(AdvanceArmed)"), 
                      _("Arm\nStart"), Size);
      invalid = false;
      break;
    case TaskAdvance::TURN_ARMED:
      ReplaceInString(OutBuffer, _T("$(AdvanceArmed)"), 
                      _("Abort\nTurn"), Size);
      invalid = false;
      break;
    case TaskAdvance::TURN_DISARMED:
      ReplaceInString(OutBuffer, _T("$(AdvanceArmed)"), 
                      _("Arm\nTurn"), Size);
      invalid = false;
      break;
    default:
      assert(1);
    }
  }

  if (_tcsstr(OutBuffer, _T("$(CheckAutoMc)"))) {
    if (!calculated.task_stats.task_valid
        && ((settings_computer.auto_mc_mode==TaskBehaviour::AUTOMC_FINALGLIDE)
            || (settings_computer.auto_mc_mode==TaskBehaviour::AUTOMC_BOTH)))
      invalid = true;

    ReplaceInString(OutBuffer, _T("$(CheckAutoMc)"), _T(""), Size);
  }

  if (_tcsstr(OutBuffer, _T("$(TaskAbortToggleActionName)"))) {
    if (task_manager->is_mode(TaskManager::MODE_GOTO)) {
      CondReplaceInString(calculated.common_stats.ordered_valid,
                          OutBuffer, _T("$(TaskAbortToggleActionName)"),
                          _("Resume"), _("Abort"), Size);
    } else 
      CondReplaceInString(calculated.common_stats.mode_abort,
                          OutBuffer, _T("$(TaskAbortToggleActionName)"),
                          _("Resume"), _("Abort"), Size);
  }

  return invalid;
}

bool
ButtonLabel::ExpandMacros(const TCHAR *In, TCHAR *OutBuffer, size_t Size)
{
  // ToDo, check Buffer Size
  bool invalid = false;
  _tcsncpy(OutBuffer, In, Size);
  OutBuffer[Size - 1] = '\0';

  if (_tcsstr(OutBuffer, _T("$(")) == NULL)
    return false;

  if (_tcsstr(OutBuffer, _T("$(CheckAirspace)"))) {
    if (airspace_database.empty())
      invalid = true;

    ReplaceInString(OutBuffer, _T("$(CheckAirspace)"), _T(""), Size);
  }

  invalid |= ExpandTaskMacros(OutBuffer, Size,
                              Calculated(), SettingsComputer());

  if (_tcsstr(OutBuffer, _T("$(CheckReplay)"))) {
    if (!Basic().gps.Replay
        && !replay.NmeaReplayEnabled()
        && Basic().gps.MovementDetected)
      invalid = true;

    ReplaceInString(OutBuffer, _T("$(CheckReplay)"), _T(""), Size);
  }

  if (_tcsstr(OutBuffer, _T("$(CheckWaypointFile)"))) {
    invalid |= way_points.empty();
    ReplaceInString(OutBuffer, _T("$(CheckWaypointFile)"), _T(""), Size);
  }

  if (_tcsstr(OutBuffer, _T("$(CheckSettingsLockout)"))) {
    if (!is_simulator() && XCSoarInterface::LockSettingsInFlight &&
        Basic().flight.Flying)
      invalid = true;

    ReplaceInString(OutBuffer, _T("$(CheckSettingsLockout)"), _T(""), Size);
  }

  if (_tcsstr(OutBuffer, _T("$(CheckFLARM)"))) {
    if (!Basic().flarm.FLARM_Available)
      invalid = true;

    ReplaceInString(OutBuffer, _T("$(CheckFLARM)"), _T(""), Size);
  }
  if (_tcsstr(OutBuffer, _T("$(CheckTerrain)"))) {
    if (!Calculated().TerrainValid)
      invalid = true;

    ReplaceInString(OutBuffer, _T("$(CheckTerrain)"), _T(""), Size);
  }

  CondReplaceInString(logger.isLoggerActive(), OutBuffer,
                      _T("$(LoggerActive)"), _("Stop"),
                      _("Start"), Size);

  if (_tcsstr(OutBuffer, _T("$(SnailTrailToggleName)"))) {
    switch(SettingsMap().TrailActive) {
    case 0:
      ReplaceInString(OutBuffer, _T("$(SnailTrailToggleName)"),
                      _("Long"), Size);
      break;
    case 1:
      ReplaceInString(OutBuffer, _T("$(SnailTrailToggleName)"),
                      _("Short"), Size);
      break;
    case 2:
      ReplaceInString(OutBuffer, _T("$(SnailTrailToggleName)"),
                      _("Full"), Size);
      break;
    case 3:
      ReplaceInString(OutBuffer, _T("$(SnailTrailToggleName)"),
                      _("Off"), Size);
      break;
    }
  }

  if (_tcsstr(OutBuffer, _T("$(AirSpaceToggleName)"))) {
    switch(SettingsMap().OnAirSpace) {
    case 0:
      ReplaceInString(OutBuffer, _T("$(AirSpaceToggleName)"),
                      _("ON"), Size);
      break;
    case 1:
      ReplaceInString(OutBuffer, _T("$(AirSpaceToggleName)"),
                      _("OFF"), Size);
      break;
    }
  }

  if (_tcsstr(OutBuffer, _T("$(TerrainTopologyToggleName)"))) {
    char val = 0;
    if (SettingsMap().EnableTopology)
      val++;
    if (SettingsMap().EnableTerrain)
      val += (char)2;
    switch (val) {
    case 0:
      ReplaceInString(OutBuffer, _T("$(TerrainTopologyToggleName)"),
                      _("Topology on"), Size);
      break;
    case 1:
      ReplaceInString(OutBuffer, _T("$(TerrainTopologyToggleName)"),
                      _("Terrain on"), Size);
      break;
    case 2:
      ReplaceInString(OutBuffer, _T("$(TerrainTopologyToggleName)"),
                      _("Terrain + Topology"), Size);
      break;
    case 3:
      ReplaceInString(OutBuffer, _T("$(TerrainTopologyToggleName)"),
                      _("Terrain off"), Size);
      break;
    }
  }

  CondReplaceInString(main_window.GetFullScreen(), OutBuffer,
                      _T("$(FullScreenToggleActionName)"),
                      _("Off"), _("On"), Size);
  CondReplaceInString(SettingsMap().AutoZoom, OutBuffer,
		                  _T("$(ZoomAutoToggleActionName)"),
		                  _("Manual"), _("Auto"), Size);
  CondReplaceInString(SettingsMap().EnableTopology, OutBuffer,
                      _T("$(TopologyToggleActionName)"),
                      _("Off"), _("On"), Size);
  CondReplaceInString(SettingsMap().EnableTerrain, OutBuffer,
                      _T("$(TerrainToggleActionName)"),
                      _("Off"), _("On"), Size);

  if (_tcsstr(OutBuffer, _T("$(MapLabelsToggleActionName)"))) {
    switch(SettingsMap().DeclutterLabels) {
    case 0:
      ReplaceInString(OutBuffer, _T("$(MapLabelsToggleActionName)"),
                      _("MID"), Size);
      break;
    case 1:
      ReplaceInString(OutBuffer, _T("$(MapLabelsToggleActionName)"),
                      _("OFF"), Size);
      break;
    case 2:
      ReplaceInString(OutBuffer, _T("$(MapLabelsToggleActionName)"),
                      _("ON"), Size);
      break;
    }
  }

  CondReplaceInString(SettingsComputer().auto_mc,
                      OutBuffer, _T("$(MacCreadyToggleActionName)"),
                      _("Manual"), _("Auto"), Size);
  CondReplaceInString(SettingsMap().EnableAuxiliaryInfo,
                      OutBuffer, _T("$(AuxInfoToggleActionName)"),
                      _("Off"), _("On"), Size);

  CondReplaceInString(SettingsMap().UserForceDisplayMode == dmCircling,
                      OutBuffer, _T("$(DispModeClimbShortIndicator)"),
                      _T("(*)"), _T(""), Size);
  CondReplaceInString(SettingsMap().UserForceDisplayMode == dmCruise,
                      OutBuffer, _T("$(DispModeCruiseShortIndicator)"),
                      _T("(*)"), _T(""), Size);
  CondReplaceInString(SettingsMap().UserForceDisplayMode == dmNone,
                      OutBuffer, _T("$(DispModeAutoShortIndicator)"),
                      _T("(*)"), _T(""), Size);
  CondReplaceInString(SettingsMap().UserForceDisplayMode == dmFinalGlide,
                      OutBuffer, _T("$(DispModeFinalShortIndicator)"),
                      _T("(*)"), _T(""), Size);

  CondReplaceInString(SettingsComputer().AltitudeMode == ALLON,
                      OutBuffer, _T("$(AirspaceModeAllShortIndicator)"),
                      _T("(*)"), _T(""), Size);
  CondReplaceInString(SettingsComputer().AltitudeMode == CLIP,
                      OutBuffer, _T("$(AirspaceModeClipShortIndicator)"),
                      _T("(*)"), _T(""), Size);
  CondReplaceInString(SettingsComputer().AltitudeMode == AUTO,
                      OutBuffer, _T("$(AirspaceModeAutoShortIndicator)"),
                      _T("(*)"), _T(""), Size);
  CondReplaceInString(SettingsComputer().AltitudeMode == ALLBELOW,
                      OutBuffer, _T("$(AirspaceModeBelowShortIndicator)"),
                      _T("(*)"), _T(""), Size);
  CondReplaceInString(SettingsComputer().AltitudeMode == ALLOFF,
                      OutBuffer, _T("$(AirspaceModeAllOffIndicator)"),
                      _T("(*)"), _T(""), Size);

  CondReplaceInString(SettingsMap().TrailActive == 0,
                      OutBuffer, _T("$(SnailTrailOffShortIndicator)"),
                      _T("(*)"), _T(""), Size);
  CondReplaceInString(SettingsMap().TrailActive == 2,
                      OutBuffer, _T("$(SnailTrailShortShortIndicator)"),
                      _T("(*)"), _T(""), Size);
  CondReplaceInString(SettingsMap().TrailActive == 1,
                      OutBuffer, _T("$(SnailTrailLongShortIndicator)"),
                      _T("(*)"), _T(""), Size);
  CondReplaceInString(SettingsMap().TrailActive == 3,
                      OutBuffer, _T("$(SnailTrailFullShortIndicator)"),
                      _T("(*)"), _T(""), Size);

  CondReplaceInString(SettingsMap().OnAirSpace == 0,
                      OutBuffer, _T("$(AirSpaceOffShortIndicator)"),
                      _T("(*)"), _T(""), Size);
  CondReplaceInString(SettingsMap().OnAirSpace == 1,
                      OutBuffer, _T("$(AirSpaceOnShortIndicator)"),
                      _T("(*)"), _T(""), Size);

  CondReplaceInString(SettingsMap().EnableFLARMGauge != 0,
                      OutBuffer, _T("$(FlarmDispToggleActionName)"),
                      _("Off"), _("On"), Size);

  return invalid;
}
