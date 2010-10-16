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

#if !defined(XCSOAR_GLIDECOMPUTER_AIRDATA_HPP)
#define XCSOAR_GLIDECOMPUTER_AIRDATA_HPP

#include "GlideComputerBlackboard.hpp"
#include "GlideRatio.hpp"
#include "ThermalLocator.h"
#include "Wind/WindAnalyser.hpp"
#include "GPSClock.hpp"
#include "Math/SunEphemeris.hpp"
#include "Util/WindowFilter.hpp"

class GlidePolar;
class ProtectedAirspaceWarningManager;
class ProtectedTaskManager;

// TODO: replace copy constructors so copies of these structures
// do not replicate the large items or items that should be singletons
// OR: just make them static?

class TaskClientCalc;

class GlideComputerAirData: virtual public GlideComputerBlackboard {
public:
  GlideComputerAirData(ProtectedAirspaceWarningManager &_awm,
                       ProtectedTaskManager &_task);

  GlideRatioCalculator rotaryLD;
  SunEphemeris sun;
  virtual void ProcessIdle();

  void SetWindEstimate(fixed wind_speed, fixed wind_bearing,
		       const int quality=3); // JMW check
  WindAnalyser   windanalyser; // JMW TODO, private and lock-protected

  GlidePolar get_glide_polar() const;

private:
  ThermalLocator thermallocator;
protected:
  ProtectedAirspaceWarningManager &m_airspace;

  void ResetFlight(const bool full=true);
  void ProcessBasic();
  void ProcessVertical();

  virtual void OnTakeoff();
  virtual void OnLanding();
  virtual void OnDepartedThermal();
  virtual void OnSwitchClimbMode(bool isclimb, bool left);

  bool FlightTimes();
private:
  void DoWindCirclingMode(const bool left);
  void DoWindCirclingSample();
  void DoWindCirclingAltitude();
  void AverageClimbRate();
  void Average30s();
  void AverageThermal();
  void ResetLiftDatabase();
  void UpdateLiftDatabase();
  void MaxHeightGain();
  void ThermalGain();
  void LD();
  void CruiseLD();
  void Wind();
  void TerrainHeight();
  void TakeoffLanding();
  void AirspaceWarning();
  void TerrainFootprint(const fixed max_dist);
  void BallastDump();
  void ThermalSources();

  /**
   * Updates stats during transition from climb mode to cruise mode
   * Sets last thermal stats if Circling state has changed
   * Inputs:
   *  CruiseStartTime
   *  ClimbStartTime
   *  CruiseStartAlt
   *  Basic().EnergyHeight
   * Updates:
   *  LastThermalAverage
   *  LastThermalGain
   *  LastThermalTime
   *  LastThermalAverageSmooth
   */
  void LastThermalStats();
  void ThermalBand();
  void PercentCircling(const fixed Rate);
  void Turning();
  void ProcessSun();
  GPSClock airspace_clock;
  GPSClock ballast_clock;

  WindowFilter vario_30s_filter;
  WindowFilter netto_30s_filter;
};

#endif

