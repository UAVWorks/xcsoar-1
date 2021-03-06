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

#ifndef XCSOAR_GLIDECOMPUTER_STATS_HPP
#define XCSOAR_GLIDECOMPUTER_STATS_HPP

#include "GlideComputerBlackboard.hpp"
#include "FlightStatistics.hpp"
#include "GPSClock.hpp"

class ProtectedTaskManager;

class GlideComputerStats:
  virtual public GlideComputerBlackboard
{
public:
  GlideComputerStats(ProtectedTaskManager &task);
  FlightStatistics flightstats;

protected:
  void ResetFlight(const bool full = true);
  void StartTask();
  bool DoLogging();
  void SetFastLogging();

protected:
  virtual void OnClimbBase(fixed StartAlt);
  virtual void OnClimbCeiling();
  virtual void OnDepartedThermal();

private:
  GPSClock log_clock;
  GPSClock stats_clock;
  /** number of points to log at high rate */
  unsigned FastLogNum;
};

#endif

