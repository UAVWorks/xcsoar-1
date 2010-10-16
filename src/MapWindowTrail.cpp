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

#include "MapWindow.hpp"
#include "Math/Earth.hpp"
#include "Screen/Layout.hpp"
#include "Screen/Util.hpp"
#include "Screen/Graphics.hpp"
#include "Task/ProtectedTaskManager.hpp"

#include <algorithm>

using std::min;
using std::max;

gcc_const
static int
fSnailColour(fixed cv)
{
  return max((short)0, min((short)(NUMSNAILCOLORS - 1),
                           (short)((cv + fixed_one) / 2 * NUMSNAILCOLORS)));
}

void
MapWindow::DrawTrail(Canvas &canvas) const
{
  if (!SettingsMap().TrailActive || task == NULL)
    return;

  const MapWindowProjection &projection = render_projection;

  unsigned min_time = 0;

  if (projection.GetDisplayMode() == dmCircling) {
    min_time = max(0, (int)Basic().Time - 600);
  } else {
    switch(SettingsMap().TrailActive) {
    case 1:
      min_time = max(0, (int)Basic().Time - 3600);
      break;
    case 2:
      min_time = max(0, (int)Basic().Time - 600);
      break;
    case 0:
      min_time = 0; // full
      break;
    }
  }

  TracePointVector trace =
    task->find_trace_points(projection.GetPanLocation(),
                            projection.GetScreenDistanceMeters(),
                            min_time,
                            projection.DistancePixelsToMeters(3));

  if (trace.empty()) return; // nothing to draw

  GeoPoint traildrift;

  if (SettingsMap().EnableTrailDrift &&
      projection.GetDisplayMode() == dmCircling) {
    GeoPoint tp1;

    FindLatitudeLongitude(Basic().Location,
                          Basic().wind.bearing,
                          Basic().wind.norm,
                          &tp1);
    traildrift = Basic().Location - tp1;
  }

  fixed vario_max = fixed(0.75);
  fixed vario_min = fixed(-2.0);

  for (TracePointVector::const_iterator it = trace.begin();
       it != trace.end(); ++it) {
    vario_max = max(it->NettoVario, vario_max);
    vario_min = min(it->NettoVario, vario_min);
  };

  vario_max = min(fixed(7.5), vario_max);
  vario_min = max(fixed(-5.0), vario_min);

  unsigned last_time = 0;

  for (TracePointVector::const_iterator it = trace.begin();
       it != trace.end(); ++it) {

    const fixed dt = (Basic().Time - fixed(it->time)) * it->drift_factor;
    POINT pt = projection.LonLat2Screen(it->get_location().parametric(traildrift, dt));

    if (it->last_time != last_time) {
      canvas.move_to(pt.x, pt.y);
    } else {

      const fixed colour_vario = negative(it->NettoVario)?
        -it->NettoVario/vario_min :
        it->NettoVario/vario_max ;

      canvas.select(Graphics::hSnailPens[fSnailColour(colour_vario)]);
      canvas.line_to(pt.x, pt.y);
    }
    last_time = it->time;
  }
  canvas.line_to(projection.GetOrigAircraft().x,
                 projection.GetOrigAircraft().y);
}


