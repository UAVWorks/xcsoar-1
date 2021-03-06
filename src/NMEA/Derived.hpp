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

#ifndef XCSOAR_NMEA_DERIVED_H
#define XCSOAR_NMEA_DERIVED_H

#include "Math/fixed.hpp"
#include "Navigation/GeoPoint.hpp"
#include "Navigation/SpeedVector.hpp"
#include "Task/TaskStats/TaskStats.hpp"
#include "Task/TaskStats/CommonStats.hpp"
#include "NMEA/ThermalBand.hpp"
#include "TeamCodeCalculation.h"

#include <tchar.h>

#define MAX_THERMAL_SOURCES 20

/**
 * Structure to hold information on identified thermal sources on the ground
 */
struct THERMAL_SOURCE_INFO
{
  GeoPoint Location;
  fixed GroundHeight;
  fixed LiftRate;
  fixed Time;
};

/**
 * Enumeration for cruise/circling mode detection
 * 
 */
typedef enum {
  CRUISE= 0,                    /**< Established cruise mode */
  WAITCLIMB,                    /**< In cruise, pending transition to climb */
  CLIMB,                        /**< Established climb mode */
  WAITCRUISE                    /**< In climb, pending transition to cruise */
} CirclingMode_t;

/**
 * Derived vario data
 * 
 */
struct VARIO_INFO
{
  /** Average vertical speed based on 30s */
  fixed Average30s;
  /** Average vertical speed of the airmass based on 30s */
  fixed NettoAverage30s;

  /** Instant glide ratio */
  fixed LD;
  /** Glide ratio while in Cruise mode */
  fixed CruiseLD;
  /** Average glide ratio */
  int AverageLD;

  fixed LDvario;

  /**
   * The lift of each ten degrees while circling.
   * Index 1 equals 5 to 15 degrees.
   */
  fixed LiftDatabase[36];
};

/**
 * Derived climb data
 * 
 */
struct CLIMB_INFO
{
  /** Average vertical speed in the thermal */
  fixed ThermalAverage;
  /** Average vertical speed in the thermal (minimum 0.0) */
  fixed ThermalAverageAdjusted;

  /** Altitude gained while in the thermal */
  fixed ThermalGain;

  /** Average vertical speed in the last thermal */
  fixed LastThermalAverage;
  /** Altitude gained while in the last thermal */
  fixed LastThermalGain;
  /** Time spend in the last thermal */
  fixed LastThermalTime;

  /** Average vertical speed in the last thermals smoothed by low-pass-filter */
  fixed LastThermalAverageSmooth;
};


/**
 * Data for tracking of climb/cruise mode and transition points
 * 
 */
struct CIRCLING_INFO
{

  /** Turn rate after low pass filter */
  fixed SmoothedTurnRate;

  /** StartLocation of the current/last climb */
  GeoPoint ClimbStartLocation;
  /** StartAltitude of the current/last climb */
  fixed ClimbStartAlt;
  /** StartTime of the current/last climb */
  fixed ClimbStartTime;

  /** StartLocation of the current/last cruise */
  GeoPoint CruiseStartLocation;
  /** StartAltitude of the current/last cruise */
  fixed CruiseStartAlt;
  /** StartTime of the current/last cruise */
  fixed CruiseStartTime;

  /** Start/End time of the turn (used for flight mode determination) */
  fixed TurnStartTime;
  /** Start/End location of the turn (used for flight mode determination) */
  GeoPoint TurnStartLocation;
  /** Start/End altitude of the turn (used for flight mode determination) */
  fixed TurnStartAltitude;
  /** Start/End energy height of the turn (used for flight mode determination) */
  fixed TurnStartEnergyHeight;

  /** Current TurnMode (Cruise, Climb or somewhere between) */
  CirclingMode_t TurnMode;

  /** True if in circling mode, False otherwise */
  bool Circling;

  /** Circling/Cruise ratio in percent */
  fixed PercentCircling;

  /** Time spent in cruise mode */
  fixed timeCruising;
  /** Time spent in circling mode */
  fixed timeCircling;

  /** Minimum altitude since start of task */
  fixed MinAltitude;

  /** Maximum height gain (from MinAltitude) during task */
  fixed MaxHeightGain;

  /** Total height climbed during task */
  fixed TotalHeightClimb;
};

/**
 * Derived terrain altitude information, including glide range
 * 
 */
struct TERRAIN_ALT_INFO
{
  enum {
    /** number of radials to do range footprint calculation on */
    NUMTERRAINSWEEPS = 20,
  };

  /** Terrain altitude */
  fixed TerrainAlt;
  /** True if terrain is valid, False otherwise */
  bool   TerrainValid;

  GeoPoint TerrainWarningLocation;

  /** Final glide ground line */
  GeoPoint GlideFootPrint[NUMTERRAINSWEEPS];

  /** Lowest height within glide range */
  fixed TerrainBase;
};

/**
 * Derived climb rate history
 * 
 */
struct CLIMB_HISTORY_INFO
{
  /** Average climb rate for each episode */
  fixed AverageClimbRate[200];
  /** Number of samples in each episode */
  long AverageClimbRateN[200];
};


/**
 * Structure for current thermal estimate from ThermalLocator
 * 
 */
struct THERMAL_LOCATOR_INFO
{
  /** Location of thermal at aircraft altitude */
  GeoPoint ThermalEstimate_Location;
  /** Is thermal estimation valid? */
  bool ThermalEstimate_Valid;

  /** Position and data of the last thermal sources */
  THERMAL_SOURCE_INFO ThermalSources[MAX_THERMAL_SOURCES];
};

/**
 * Derived team code information
 */
struct TEAMCODE_INFO
{
  /** Team code */
  TeamCode OwnTeamCode;
  /** Bearing to the chosen team mate */
  Angle TeammateBearing;
  /** Distance to the chosen team mate */
  fixed TeammateRange;
  /** Position of the chosen team mate */
  GeoPoint TeammateLocation;
};

/**
 * A struct that holds all the calculated values derived from the data in the
 * NMEA_INFO struct
 */
struct DERIVED_INFO: 
  public VARIO_INFO,
  public CLIMB_INFO,
  public CIRCLING_INFO,
  public TERRAIN_ALT_INFO,
  public THERMAL_LOCATOR_INFO,
  public TEAMCODE_INFO,
  public CLIMB_HISTORY_INFO
{
  /**
   * @todo Reset to cleared state
   */
  void reset() {
    task_stats.reset();
    common_stats.reset();
  };

  fixed V_stf; /**< Speed to fly block/dolphin (m/s) */

  SpeedVector estimated_wind;   /**< Wind speed, direction */

  fixed AutoZoomDistance; /**< Distance to zoom to for autozoom */

  fixed TimeSunset; /**< Local time of sunset */

  TaskStats task_stats; /**< Copy of task statistics data for active task */
  CommonStats common_stats; /**< Copy of common task statistics data */

  ThermalBandInfo thermal_band;

  unsigned time_process_gps; /**< Time (ms) to process main computer functions */
  unsigned time_process_idle; /**< Time (ms) to process idle computer functions */

  fixed Experimental; /**< Used as temporary holder for new features */
};

#endif

