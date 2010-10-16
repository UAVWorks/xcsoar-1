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

#include "GlideComputerAirData.hpp"
#include "Wind/WindZigZag.hpp"
#include "Wind/WindAnalyser.hpp"
#include "GlideComputer.hpp"
#include "Protection.hpp"
#include "SettingsComputer.hpp"
#include "SettingsMap.hpp"
#include "Math/LowPassFilter.hpp"
#include "Math/Earth.hpp"
#include "Math/Constants.h"
#include "Terrain/RasterTerrain.hpp"
#include "Terrain/GlideTerrain.hpp"
#include "LocalTime.hpp"
#include "MapWindowProjection.hpp"
#include "Components.hpp"
#include "Interface.hpp"
#include "Atmosphere.h"
#include "LogFile.hpp"
#include "GPSClock.hpp"
#include "ThermalBase.hpp"
#include "GlideSolvers/GlidePolar.hpp"
#include "Airspace/ProtectedAirspaceWarningManager.hpp"
#include "Task/ProtectedTaskManager.hpp"
#include "Engine/Airspace/Airspaces.hpp"
#include "Defines.h"

#include <algorithm>

using std::min;
using std::max;

static const fixed MinTurnRate(4);
static const fixed CruiseClimbSwitch(15);
static const fixed ClimbCruiseSwitch(10);
static const fixed THERMAL_TIME_MIN(45);

GlideComputerAirData::GlideComputerAirData(ProtectedAirspaceWarningManager &awm,
                                           ProtectedTaskManager& _task):
  GlideComputerBlackboard(_task),
  m_airspace(awm),
  // scan airspace every 2 seconds
  airspace_clock(fixed_two),
  // only update every 5 seconds to stop flooding the devices
  ballast_clock(fixed(5)),
  vario_30s_filter(30),
  netto_30s_filter(30)
{
  rotaryLD.init(SettingsComputer());

  // JMW TODO enhancement: seed initial wind store with start conditions
  // SetWindEstimate(Calculated().WindSpeed, Calculated().WindBearing, 1);
}

void
GlideComputerAirData::ResetFlight(const bool full)
{
  const AIRCRAFT_STATE as = ToAircraftState(Basic());
  m_airspace.reset_warning(as);

  vario_30s_filter.reset();
  netto_30s_filter.reset();

  ResetLiftDatabase();
}

/**
 * Calculates some basic values
 */
void
GlideComputerAirData::ProcessBasic()
{
  TerrainHeight();
  ProcessSun();
  SetCalculated().ThermalAverageAdjusted = GetAverageThermal();
}

/**
 * Calculates some other values
 */
void
GlideComputerAirData::ProcessVertical()
{
  Turning();
  Wind();

  thermallocator.Process(Calculated().Circling,
                         Basic().Time, Basic().Location,
                         Basic().NettoVario,
                         Basic().wind, SetCalculated());

  CuSonde::updateMeasurements(Basic());
  LastThermalStats();
  LD();
  CruiseLD();

  if (!Basic().flight.OnGround && !Calculated().Circling)
    SetCalculated().AverageLD = rotaryLD.calculate();

  Average30s();
  AverageClimbRate();
  ThermalGain();
  AverageThermal();
  UpdateLiftDatabase();
}

/**
 * Calculates the wind
 */
void
GlideComputerAirData::Wind()
{
  if (!Basic().flight.Flying || !time_advanced())
    return;

  if (Calculated().TurnMode == CLIMB)
    DoWindCirclingSample();

  // generate new wind vector if altitude changes or a new
  // estimate is available
  DoWindCirclingAltitude();

  // update zigzag wind
  if ((SettingsComputer().AutoWindMode & D_AUTOWIND_ZIGZAG)
      && !Basic().gps.Replay
      && (Basic().TrueAirspeed > m_task.get_glide_polar().get_Vtakeoff())) {
    fixed zz_wind_speed;
    fixed zz_wind_bearing;
    int quality;
    quality = WindZigZagUpdate(Basic(), Calculated(),
                               zz_wind_speed, zz_wind_bearing);

    if (quality > 0)
      SetWindEstimate(zz_wind_speed, zz_wind_bearing, quality);
  }
}

/**
 * Passes data to the windanalyser.slot_newFlightMode method
 */
void
GlideComputerAirData::DoWindCirclingMode(const bool left)
{
  if (SettingsComputer().AutoWindMode & D_AUTOWIND_CIRCLING)
    windanalyser.slot_newFlightMode(Basic(), Calculated(), left, 0);
}

/**
 * Passes data to the windanalyser.slot_newSample method
 */
void
GlideComputerAirData::DoWindCirclingSample()
{
  if (SettingsComputer().AutoWindMode & D_AUTOWIND_CIRCLING)
    windanalyser.slot_newSample(Basic(), SetCalculated());
}

/**
 * Passes data to the windanalyser.SlotAltitude method
 */
void
GlideComputerAirData::DoWindCirclingAltitude()
{
  if (SettingsComputer().AutoWindMode & D_AUTOWIND_CIRCLING)
    windanalyser.slot_Altitude(Basic(), SetCalculated());
}

void
GlideComputerAirData::SetWindEstimate(fixed wind_speed, fixed wind_bearing,
                                      const int quality)
{
  Vector v_wind = Vector(SpeedVector(Angle::degrees(wind_bearing),
                                     wind_speed));

  windanalyser.slot_newEstimate(Basic(), SetCalculated(), v_wind, quality);
}

void
GlideComputerAirData::AverageClimbRate()
{
  if (Basic().AirspeedAvailable && Basic().TotalEnergyVarioAvailable
      && (!Calculated().Circling)) {

    int vi = iround(Basic().IndicatedAirspeed);
    if (vi <= 0 || vi >= iround(SettingsComputer().SafetySpeed))
      // out of range
      return;

    if (Basic().acceleration.Available)
      if (fabs(fabs(Basic().acceleration.Gload) - fixed_one) > fixed(0.25))
        // G factor too high
        return;

    if (positive(Basic().TrueAirspeed)) {
      // TODO: Check this is correct for TAS/IAS
      fixed ias_to_tas = Basic().IndicatedAirspeed /
        Basic().TrueAirspeed;
      fixed w_tas = Basic().TotalEnergyVario * ias_to_tas;

      SetCalculated().AverageClimbRate[vi] += w_tas;
      SetCalculated().AverageClimbRateN[vi]++;
    }
  }
}

#ifdef NEWCLIMBAV
ClimbAverageCalculator climbAverageCalculator;
void
GlideComputerAirData::Average30s()
{
  Calculated().Average30s =
    climbAverageCalculator.GetAverage(Basic().Time, Basic().Altitude, 30);
  Calculated().NettoAverage30s = Calculated().Average30s;
}
#else

void
GlideComputerAirData::Average30s()
{
  if (!time_advanced() 
      || (Calculated().Circling != LastCalculated().Circling)) {

    vario_30s_filter.reset();
    netto_30s_filter.reset();
    SetCalculated().Average30s = Basic().TotalEnergyVario;
    SetCalculated().NettoAverage30s = Basic().NettoVario;
  }

  if (!time_advanced())
    return;

  const unsigned Elapsed = (unsigned)(Basic().Time - LastBasic().Time);
  for (unsigned i = 0; i < Elapsed; ++i) {
    if (vario_30s_filter.update(Basic().TotalEnergyVario))
      SetCalculated().Average30s =
        LowPassFilter(Calculated().Average30s, vario_30s_filter.average(),
                      fixed(0.8));

    if (netto_30s_filter.update(Basic().NettoVario))
      SetCalculated().NettoAverage30s =
        LowPassFilter(Calculated().NettoAverage30s, netto_30s_filter.average(),
                      fixed(0.8));
  }
}
#endif

void
GlideComputerAirData::AverageThermal()
{
  if (positive(Calculated().ClimbStartTime) &&
      Basic().Time > Calculated().ClimbStartTime)
    SetCalculated().ThermalAverage =
      Calculated().ThermalGain / (Basic().Time - Calculated().ClimbStartTime);
}

/**
 * This function converts a heading into an unsigned index for the LiftDatabase.
 *
 * This is calculated with Angles to deal with the 360 degree limit.
 *
 * 357 = 0
 * 4 = 0
 * 5 = 1
 * 14 = 1
 * 15 = 2
 * ...
 * @param heading The heading to convert
 * @return The index for the LiftDatabase array
 */
static unsigned
heading_to_index(Angle &heading)
{
  static const Angle afive = Angle::degrees(fixed(5));

  unsigned index =
      floor((heading + afive).as_bearing().value_degrees() / 10);

  return std::max(0u, std::min(35u, index));
}

void
GlideComputerAirData::UpdateLiftDatabase()
{
  // Don't update the lift database if we are not in circling mode
  if (!Calculated().Circling)
    return;

  // If we just started circling
  // -> reset the database because this is a new thermal
  if (!LastCalculated().Circling)
    ResetLiftDatabase();

  // Determine the direction in which we are circling
  bool left = negative(Calculated().SmoothedTurnRate);

  // Depending on the direction set the step size sign for the
  // following loop
  Angle heading_step = Angle::degrees(fixed(left ? -10 : 10));

  // Start at the last heading and add heading_step until the current heading
  // is reached. For each heading save the current lift value into the
  // LiftDatabase. Last and current heading are included since they are
  // a part of the ten degree interval most of the time.
  //
  // This is done with Angles to deal with the 360 degrees limit.
  // e.g. last heading 348 degrees, current heading 21 degrees
  //
  // The loop condition stops until the current heading is reached.
  // Depending on the circling direction the current heading will be
  // smaller or bigger then the last one, because of that negative() is
  // tested against the left variable.
  for (Angle h = LastBasic().Heading;
       left == negative((Basic().Heading - h).as_delta().value_degrees());
       h += heading_step) {
    unsigned index = heading_to_index(h);
    SetCalculated().LiftDatabase[index] = Basic().TotalEnergyVario;
  }
}

void
GlideComputerAirData::ResetLiftDatabase()
{
  // Reset LiftDatabase to zero
  for (unsigned i = 0; i < 36; i++)
    SetCalculated().LiftDatabase[i] = fixed_zero;
}

void
GlideComputerAirData::MaxHeightGain()
{
  if (!Basic().flight.Flying)
    return;

  if (positive(Calculated().MinAltitude)) {
    fixed height_gain = Basic().NavAltitude
      - Calculated().MinAltitude;
    SetCalculated().MaxHeightGain = max(height_gain, Calculated().MaxHeightGain);
  } else {
    SetCalculated().MinAltitude = Basic().NavAltitude;
  }

  SetCalculated().MinAltitude = min(Basic().NavAltitude, Calculated().MinAltitude);
}

void
GlideComputerAirData::ThermalGain()
{
  if (positive(Calculated().ClimbStartTime) &&
      Basic().Time >= Calculated().ClimbStartTime)
    SetCalculated().ThermalGain =
      Basic().TEAltitude - Calculated().ClimbStartAlt;
}

void
GlideComputerAirData::LD()
{
  if (time_retreated()) {
    SetCalculated().LDvario = fixed(INVALID_GR);
    SetCalculated().LD = fixed(INVALID_GR);
  }

  if (time_advanced()) {
    fixed DistanceFlown = Distance(Basic().Location, LastBasic().Location);

    SetCalculated().LD =
      UpdateLD(Calculated().LD, DistanceFlown,
               LastBasic().NavAltitude - Basic().NavAltitude, fixed(0.1));

    if (!Basic().flight.OnGround && !Calculated().Circling)
      rotaryLD.add((int)DistanceFlown, (int)Basic().NavAltitude);
  }

  // LD instantaneous from vario, updated every reading..
  if (Basic().TotalEnergyVarioAvailable && Basic().AirspeedAvailable &&
      Basic().flight.Flying) {
    SetCalculated().LDvario =
      UpdateLD(Calculated().LDvario, Basic().IndicatedAirspeed,
               -Basic().TotalEnergyVario, fixed(0.3));
  } else {
    SetCalculated().LDvario = fixed(INVALID_GR);
  }
}

void
GlideComputerAirData::CruiseLD()
{
  if(!Calculated().Circling) {
    if (negative(Calculated().CruiseStartTime)) {
      SetCalculated().CruiseStartLocation = Basic().Location;
      SetCalculated().CruiseStartAlt = Basic().NavAltitude;
      SetCalculated().CruiseStartTime = Basic().Time;
    } else {
      fixed DistanceFlown = Distance(Basic().Location,
                                     Calculated().CruiseStartLocation);
      SetCalculated().CruiseLD =
          UpdateLD(Calculated().CruiseLD, DistanceFlown,
                   Calculated().CruiseStartAlt - Basic().NavAltitude,
                   fixed_half);
    }
  }
}

/**
 * Reads the current terrain height
 */
void
GlideComputerAirData::TerrainHeight()
{
  if (terrain == NULL) {
    SetCalculated().TerrainValid = false;
    return;
  }

  short Alt = terrain->GetTerrainHeight(Basic().Location);

  SetCalculated().TerrainValid = Alt > RasterTerrain::TERRAIN_INVALID;
  SetCalculated().TerrainAlt = fixed(std::max((short)0, Alt));
}

/**
 * 1. Detects time retreat and calls ResetFlight if GPS lost
 * 2. Detects change in replay status and calls ResetFlight if so
 * 3. Calls DetectStartTime and saves the time of flight
 * @return true as default, false if something is wrong in time
 */
bool
GlideComputerAirData::FlightTimes()
{
  if (Basic().gps.Replay != LastBasic().gps.Replay)
    // reset flight before/after replay logger
    ResetFlight(Basic().gps.Replay);

  if (positive(Basic().Time) && time_retreated()) {
    // 20060519:sgi added (Basic().Time != 0) due to always return here
    // if no GPS time available
    if (!Basic().gps.NAVWarning)
      // Reset statistics.. (probably due to being in IGC replay mode)
      ResetFlight(false);

    return false;
  }

  TakeoffLanding();

  return true;
}

void
GlideComputerAirData::ProcessIdle()
{
  BallastDump();
  TerrainFootprint(MapProjection().GetScreenDistanceMeters());
  if (airspace_clock.check_advance(Basic().Time)
      && SettingsComputer().EnableAirspaceWarnings)
    AirspaceWarning();
}

/**
 * Detects takeoff and landing events
 */
void
GlideComputerAirData::TakeoffLanding()
{
  if (Basic().GroundSpeed > fixed_one)
    // stop system from shutting down if moving
    XCSoarInterface::InterfaceTimeoutReset();

  if (Basic().flight.Flying && !LastBasic().flight.Flying)
    OnTakeoff();
  else if (!Basic().flight.Flying && LastBasic().flight.Flying)
    OnLanding();
}

void
GlideComputerAirData::OnLanding()
{
  // JMWX  restore data calculated at finish so
  // user can review flight as at finish line

  if (Calculated().common_stats.task_finished)
    RestoreFinish();
}

void
GlideComputerAirData::OnTakeoff()
{
  // reset stats on takeoff
  ResetFlight();

  // save stats in case we never finish
  SaveFinish();
}

void
GlideComputerAirData::AirspaceWarning()
{
  airspace_database.set_flight_levels(Basic().pressure);

  const AIRCRAFT_STATE as = ToAircraftState(Basic());
  if (m_airspace.update_warning(as))
    airspaceWarningEvent.trigger();
}

void
GlideComputerAirData::TerrainFootprint(fixed screen_range)
{
  if (terrain == NULL || !SettingsComputer().FinalGlideTerrain)
    return;

  // initialise base
  SetCalculated().TerrainBase = Calculated().TerrainAlt;

  // estimate max range (only interested in at most one screen
  // distance away) except we need to scan for terrain base, so 20km
  // search minimum is required

  GlidePolar glide_polar = m_task.get_glide_polar();
  glide_polar.set_mc(min(Calculated().common_stats.current_risk_mc,
                         SettingsComputer().safety_mc));

  GlideTerrain g_terrain(SettingsComputer(), *terrain);

  g_terrain.set_max_range(max(fixed(20000), screen_range));
  
  const fixed d_bearing = fixed_360 / TERRAIN_ALT_INFO::NUMTERRAINSWEEPS;

  unsigned i = 0;
  AIRCRAFT_STATE state = ToAircraftState(Basic());
  for (fixed ang = fixed_zero; ang <= fixed_360; ang += d_bearing, i++) {
    state.TrackBearing = Angle::degrees(ang);

    TerrainIntersection its = g_terrain.find_intersection(state, glide_polar);
    
    SetCalculated().GlideFootPrint[i] = its.location;
  } 

  SetCalculated().TerrainBase = g_terrain.get_terrain_base();
  SetCalculated().Experimental = Calculated().TerrainBase;
}

void
GlideComputerAirData::BallastDump()
{
  fixed dt = ballast_clock.delta_advance(Basic().Time);

  if (!SettingsComputer().BallastTimerActive || negative(dt))
    return;

  GlidePolar glide_polar = m_task.get_glide_polar();
  fixed ballast = glide_polar.get_ballast();
  fixed percent_per_second =
    fixed_one / max(10, SettingsComputer().BallastSecsToEmpty);

  ballast -= dt * percent_per_second;
  if (negative(ballast)) {
    XCSoarInterface::SetSettingsComputer().BallastTimerActive = false;
    ballast = fixed_zero;
  }

  glide_polar.set_ballast(ballast);
  m_task.set_glide_polar(glide_polar);
}

void
GlideComputerAirData::OnSwitchClimbMode(bool isclimb, bool left)
{
  rotaryLD.init(SettingsComputer());

  // Tell the windanalyser of the new flight mode
  DoWindCirclingMode(left);
}

/**
 * Calculate the circling time percentage and call the thermal band calculation
 * @param Rate Current turn rate
 */
void
GlideComputerAirData::PercentCircling(const fixed Rate)
{
  // TODO accuracy: TB: this would only work right if called every ONE second!

  // JMW circling % only when really circling,
  // to prevent bad stats due to flap switches and dolphin soaring

  // if (Circling)
  if (Calculated().Circling && (Rate > MinTurnRate)) {
    // Add one second to the circling time
    // timeCircling += (Basic->Time-LastTime);
    SetCalculated().timeCircling += fixed_one;

    // Add the Vario signal to the total climb height
    SetCalculated().TotalHeightClimb += Basic().GPSVario;

    // call ThermalBand function here because it is then explicitly
    // tied to same condition as %circling calculations
    ThermalBand();
  } else {
    // Add one second to the cruise time
    // timeCruising += (Basic->Time-LastTime);
    SetCalculated().timeCruising += fixed_one;
  }

  // Calculate the circling percentage
  if (Calculated().timeCruising + Calculated().timeCircling > fixed_one)
    SetCalculated().PercentCircling = 100 * (Calculated().timeCircling) /
        (Calculated().timeCruising + Calculated().timeCircling);
  else
    SetCalculated().PercentCircling = fixed_zero;
}

/**
 * Calculates the turn rate and the derived features.
 * Determines the current flight mode (cruise/circling).
 */
void
GlideComputerAirData::Turning()
{
  // You can't be circling unless you're flying
  if (!Basic().flight.Flying || !time_advanced())
    return;

  // JMW limit rate to 50 deg per second otherwise a big spike
  // will cause spurious lock on circling for a long time
  fixed Rate = max(fixed(-50), min(fixed(50), Basic().TurnRate));

  // average rate, to detect essing
  // TODO: use rotary buffer
  static fixed rate_history[60];
  fixed rate_ave = fixed_zero;
  for (int i = 59; i > 0; i--) {
    rate_history[i] = rate_history[i - 1];
    rate_ave += rate_history[i];
  }
  rate_history[0] = Rate;
  rate_ave /= 60;

  // Make the turn rate more smooth using the LowPassFilter
  Rate = LowPassFilter(LastCalculated().SmoothedTurnRate, Rate, fixed(0.3));
  SetCalculated().SmoothedTurnRate = Rate;

  // Determine which direction we are circling
  bool LEFT = false;
  if (negative(Rate)) {
    LEFT = true;
    Rate *= -1;
  }

  // Calculate circling time percentage and call thermal band calculation
  PercentCircling(Rate);

  // Force cruise or climb mode if external device says so
  bool forcecruise = false;
  bool forcecircling = false;
  if (SettingsComputer().EnableExternalTriggerCruise && !Basic().gps.Replay) {
    forcecircling = triggerClimbEvent.test();
    forcecruise = !forcecircling;
  }

  switch (Calculated().TurnMode) {
  case CRUISE:
    // If (in cruise mode and beginning of circling detected)
    if ((Rate >= MinTurnRate) || (forcecircling)) {
      // Remember the start values of the turn
      SetCalculated().TurnStartTime = Basic().Time;
      SetCalculated().TurnStartLocation = Basic().Location;
      SetCalculated().TurnStartAltitude = Basic().NavAltitude;
      SetCalculated().TurnStartEnergyHeight = Basic().EnergyHeight;
      SetCalculated().TurnMode = WAITCLIMB;
    }
    if (!forcecircling)
      break;

  case WAITCLIMB:
    if (forcecruise) {
      SetCalculated().TurnMode = CRUISE;
      break;
    }
    if ((Rate >= MinTurnRate) || (forcecircling)) {
      if (((Basic().Time - Calculated().TurnStartTime) > CruiseClimbSwitch)
          || forcecircling) {
        // yes, we are certain now that we are circling
        SetCalculated().Circling = true;

        // JMW Transition to climb
        SetCalculated().TurnMode = CLIMB;

        // Remember the start values of the climbing period
        SetCalculated().ClimbStartLocation = Calculated().TurnStartLocation;
        SetCalculated().ClimbStartAlt = Calculated().TurnStartAltitude
            + Calculated().TurnStartEnergyHeight;
        SetCalculated().ClimbStartTime = Calculated().TurnStartTime;

        // set altitude for start of circling (as base of climb)
        OnClimbBase(Calculated().TurnStartAltitude);

        // consider code: InputEvents GCE - Move this to InputEvents
        // Consider a way to take the CircleZoom and other logic
        // into InputEvents instead?
        // JMW: NO.  Core functionality must be built into the
        // main program, unable to be overridden.
        OnSwitchClimbMode(true, LEFT);
      }
    } else {
      // nope, not turning, so go back to cruise
      SetCalculated().TurnMode = CRUISE;
    }
    break;

  case CLIMB:
    if ((Rate < MinTurnRate) || (forcecruise)) {
      // Remember the end values of the turn
      SetCalculated().TurnStartTime = Basic().Time;
      SetCalculated().TurnStartLocation = Basic().Location;
      SetCalculated().TurnStartAltitude = Basic().NavAltitude;
      SetCalculated().TurnStartEnergyHeight = Basic().EnergyHeight;

      // JMW Transition to cruise, due to not properly turning
      SetCalculated().TurnMode = WAITCRUISE;
    }
    if (!forcecruise)
      break;

  case WAITCRUISE:
    if (forcecircling) {
      SetCalculated().TurnMode = CLIMB;
      break;
    }
    if((Rate < MinTurnRate) || forcecruise) {
      if (((Basic().Time - Calculated().TurnStartTime) > ClimbCruiseSwitch)
          || forcecruise) {
        // yes, we are certain now that we are cruising again
        SetCalculated().Circling = false;

        // Transition to cruise
        SetCalculated().TurnMode = CRUISE;
        SetCalculated().CruiseStartLocation = Calculated().TurnStartLocation;
        SetCalculated().CruiseStartAlt = Calculated().TurnStartAltitude;
        SetCalculated().CruiseStartTime = Calculated().TurnStartTime;

        OnClimbCeiling();

        OnSwitchClimbMode(false, LEFT);
      }
    } else {
      // nope, we are circling again
      // JMW Transition back to climb, because we are turning again
      SetCalculated().TurnMode = CLIMB;
    }
    break;

  default:
    // error, go to cruise
    SetCalculated().TurnMode = CRUISE;
  }
}

void
GlideComputerAirData::ThermalSources()
{
  GeoPoint ground_location;
  fixed ground_altitude;

  EstimateThermalBase(Calculated().ThermalEstimate_Location,
                      Basic().NavAltitude,
                      Calculated().LastThermalAverage,
                      Basic().wind,
                      &ground_location,
                      &ground_altitude);

  if (positive(ground_altitude)) {
    fixed tbest = fixed_zero;
    int ibest = 0;

    for (int i = 0; i < MAX_THERMAL_SOURCES; i++) {
      if (negative(Calculated().ThermalSources[i].LiftRate)) {
        ibest = i;
        break;
      }
      fixed dt = Basic().Time - Calculated().ThermalSources[i].Time;
      if (dt > tbest) {
        tbest = dt;
        ibest = i;
      }
    }

    SetCalculated().ThermalSources[ibest].LiftRate = Calculated().LastThermalAverage;
    SetCalculated().ThermalSources[ibest].Location = ground_location;
    SetCalculated().ThermalSources[ibest].GroundHeight = ground_altitude;
    SetCalculated().ThermalSources[ibest].Time = Basic().Time;
  }
}

void
GlideComputerAirData::LastThermalStats()
{
  if((Calculated().Circling == false) && (LastCalculated().Circling == true)
     && positive(Calculated().ClimbStartTime)) {

    fixed ThermalTime =
        Calculated().CruiseStartTime - Calculated().ClimbStartTime;

    if (positive(ThermalTime)) {
      fixed ThermalGain = Calculated().CruiseStartAlt
          + Basic().EnergyHeight - Calculated().ClimbStartAlt;

      if (positive(ThermalGain) && (ThermalTime > THERMAL_TIME_MIN)) {
        SetCalculated().LastThermalAverage = ThermalGain / ThermalTime;
        SetCalculated().LastThermalGain = ThermalGain;
        SetCalculated().LastThermalTime = ThermalTime;

        if (Calculated().LastThermalAverageSmooth == fixed_zero)
          SetCalculated().LastThermalAverageSmooth =
              Calculated().LastThermalAverage;
        else
          SetCalculated().LastThermalAverageSmooth =
              LowPassFilter(Calculated().LastThermalAverageSmooth,
                            Calculated().LastThermalAverage, fixed(0.3));

        OnDepartedThermal();
      }
    }
  }
}

void
GlideComputerAirData::OnDepartedThermal()
{
  ThermalSources();
}

void
GlideComputerAirData::ThermalBand()
{
  if (!time_advanced())
    return;

  // JMW TODO accuracy: Should really work out dt here,
  //           but i'm assuming constant time steps

  const fixed dheight = Basic().working_band_height;

  if (!positive(Basic().working_band_height))
    return; // nothing to do.

  ThermalBandInfo &tbi = SetCalculated().thermal_band;
  if (tbi.MaxThermalHeight == fixed_zero)
    tbi.MaxThermalHeight = dheight;

  // only do this if in thermal and have been climbing
  if ((!Calculated().Circling) || negative(Calculated().Average30s))
    return;

  if (dheight > tbi.MaxThermalHeight) {
    // moved beyond ceiling, so redistribute buckets
    ThermalBandInfo new_tbi;

    // calculate new buckets so glider is below max
    fixed hbuk = tbi.MaxThermalHeight / NUMTHERMALBUCKETS;

    new_tbi.clear();
    new_tbi.MaxThermalHeight = max(fixed_one, tbi.MaxThermalHeight);
    while (new_tbi.MaxThermalHeight < dheight) {
      new_tbi.MaxThermalHeight += hbuk;
    }

    // shift data into new buckets
    for (unsigned i = 0; i < NUMTHERMALBUCKETS; i++) {
      fixed h = tbi.bucket_height(i);
      // height of center of bucket
      unsigned j = new_tbi.bucket_for_height(h);
      if (tbi.ThermalProfileN[i] > 0) {
        new_tbi.ThermalProfileW[j] += tbi.ThermalProfileW[i];
        new_tbi.ThermalProfileN[j] += tbi.ThermalProfileN[i];
      }
    }

    tbi = new_tbi;
  }

  tbi.add(dheight, Basic().TotalEnergyVario);
}

void
GlideComputerAirData::ProcessSun()
{
  sun.CalcSunTimes(Basic().Location, Basic().DateTime,
                   fixed(GetUTCOffset()) / 3600);
  SetCalculated().TimeSunset = fixed(sun.TimeOfSunSet);
}

GlidePolar 
GlideComputerAirData::get_glide_polar() const
{
  return m_task.get_glide_polar();
}
