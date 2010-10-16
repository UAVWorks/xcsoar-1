/* Copyright_License {

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
#include "ElementStat.hpp"
#include "Navigation/Aircraft.hpp"
#include <algorithm>

ElementStat::ElementStat():
  TimeStarted(-1.0),
  TimeElapsed(0.0),
  TimeRemaining(0.0),
  TimePlanned(0.0),
  gradient(0.0)
{


}


void 
ElementStat::set_times(const fixed ts,
                       const AIRCRAFT_STATE& state)
{
  TimeStarted = ts;
  if (negative(TimeStarted))
    /* not yet started */
    TimeElapsed = fixed_zero;
  else
    TimeElapsed = max(state.Time - fixed(ts), fixed_zero);

  TimeRemaining = solution_remaining.TimeElapsed;
  TimePlanned = TimeElapsed+TimeRemaining;
}

void
ElementStatComputer::reset()
{
  initialised = false;

  calc_speeds(fixed_zero);
}

void 
ElementStatComputer::calc_speeds(const fixed dt)
{
  remaining_effective.calc_speed(data.TimeRemaining);
  remaining.calc_speed(data.TimeRemaining);
  planned.calc_speed(data.TimePlanned);
  travelled.calc_speed(data.TimeElapsed);

  if (!initialised) {
    if (positive(dt) && data.TimeElapsed > fixed(15)) {
      initialised=true;
    }
    data.vario.reset(data.solution_remaining);
    remaining_effective.calc_incremental_speed(fixed_zero);
    remaining.calc_incremental_speed(fixed_zero);
    planned.calc_incremental_speed(fixed_zero);
    travelled.calc_incremental_speed(fixed_zero);
  } else {
    remaining_effective.calc_incremental_speed(dt);
    remaining.calc_incremental_speed(dt);
    planned.calc_incremental_speed(dt);
    travelled.calc_incremental_speed(dt);
    data.vario.update(data.solution_remaining, fixed(dt));
  }
}

ElementStatComputer::ElementStatComputer(ElementStat &_data)
  :data(_data),
   remaining_effective(data.remaining_effective),
   remaining(data.remaining),
   planned(data.planned),
   travelled(data.travelled, false),
   initialised(false)
{
}
