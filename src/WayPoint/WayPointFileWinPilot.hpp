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


#ifndef WAYPOINTFILEWINPILOT_HPP
#define WAYPOINTFILEWINPILOT_HPP

#include "WayPointFile.hpp"

class TextWriter;

/** 
 * Waypoint file read/writer for WinPilot format
 */
class WayPointFileWinPilot: 
  public WayPointFile 
{
public:
  WayPointFileWinPilot(const TCHAR* file_name, const int _file_num,
                       const bool _compressed=false): WayPointFile(file_name,
                                                                   _file_num,
                                                                   _compressed) {};

protected:
  bool parseLine(const TCHAR* line, const unsigned linenum,
                 Waypoints &way_points, const RasterTerrain *terrain);

  void saveFile(TextWriter &writer, const Waypoints &way_points);
  bool IsWritable() { return true; }

private:
  static bool parseString(const TCHAR* src, tstring& dest);
  static bool parseAngle(const TCHAR* src, Angle& dest, const bool lat);
  static bool parseAltitude(const TCHAR* src, fixed& dest);
  static bool parseFlags(const TCHAR* src, WaypointFlags& dest);

  static void composeLine(TextWriter &writer, const Waypoint& wp);
  static void composeAngle(TextWriter &writer,
                           const Angle& src, const bool lat);
  static void composeAltitude(TextWriter &writer, const fixed src);
  static void composeFlags(TextWriter &writer, const WaypointFlags &src);
};

#endif
