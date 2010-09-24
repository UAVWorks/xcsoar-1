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

#include "MapWindowLabels.hpp"
#include "MapWindow.hpp"

#include <stdlib.h>
#include <assert.h>

typedef struct
{
  TCHAR Name[NAME_SIZE+1];
  POINT Pos;
  TextInBoxMode_t Mode;
  int AltArivalAGL;
  bool inTask;
  bool isLandable;
  bool isAirport;
} MapWaypointLabel_t;

int _cdecl MapWaypointLabelListCompare(const void *elem1, const void *elem2);

MapWaypointLabel_t MapWaypointLabelList[50];
unsigned MapWaypointLabelListCount = 0;

int _cdecl
MapWaypointLabelListCompare(const void *elem1, const void *elem2)
{
  // Now sorts elements in task preferentially.
  /*
  if (((MapWaypointLabel_t *)elem1)->inTask && ! ((MapWaypointLabel_t *)elem2)->inTask)
    return (-1);
  */

  if (((const MapWaypointLabel_t *)elem1)->AltArivalAGL
      > ((const MapWaypointLabel_t *)elem2)->AltArivalAGL)
    return -1;

  if (((const MapWaypointLabel_t *)elem1)->AltArivalAGL
      < ((const MapWaypointLabel_t *)elem2)->AltArivalAGL)
    return 1;

  return 0;
}

void
MapWaypointLabelAdd(TCHAR *Name, int X, int Y, TextInBoxMode_t Mode,
    int AltArivalAGL, bool inTask, bool isLandable, bool isAirport,
    RECT MapRect)
{
  MapWaypointLabel_t *E;

  if ((X < MapRect.left - WPCIRCLESIZE)
      || (X > MapRect.right + (WPCIRCLESIZE * 3))
      || (Y < MapRect.top - WPCIRCLESIZE)
      || (Y > MapRect.bottom + WPCIRCLESIZE))
    return;


  if (MapWaypointLabelListCount >=
      (sizeof(MapWaypointLabelList) / sizeof(MapWaypointLabel_t)) - 1)
    return;

  E = &MapWaypointLabelList[MapWaypointLabelListCount];

  _tcscpy(E->Name, Name);
  E->Pos.x = X;
  E->Pos.y = Y;
  E->Mode = Mode;
  E->AltArivalAGL = AltArivalAGL;
  E->inTask = inTask;
  E->isLandable = isLandable;
  E->isAirport  = isAirport;

  MapWaypointLabelListCount++;
}

void MapWindow::MapWaypointLabelSortAndRender(Canvas &canvas) {
  qsort(&MapWaypointLabelList,
        MapWaypointLabelListCount,
        sizeof(MapWaypointLabel_t),
        MapWaypointLabelListCompare);

  // now draw task waypoints
  for (unsigned i = 0; i < MapWaypointLabelListCount; i++) {
    MapWaypointLabel_t *E = &MapWaypointLabelList[i];
    // draws if they are in task unconditionally,
    // otherwise, does comparison
    if (E->inTask)
      TextInBox(canvas, E->Name, E->Pos.x, E->Pos.y, E->Mode, MapRect, &label_block);
  }

  // now draw airports in order of range (closest last)
  for (unsigned i = 0; i < MapWaypointLabelListCount; i++) {
    MapWaypointLabel_t *E = &MapWaypointLabelList[i];
    // draws if they are in task unconditionally,
    // otherwise, does comparison
    if (!E->inTask && E->isAirport)
      TextInBox(canvas, E->Name, E->Pos.x, E->Pos.y, E->Mode, MapRect, &label_block);
  }

  // now draw landable waypoints in order of range (closest last)
  for (unsigned i = 0; i < MapWaypointLabelListCount; i++) {
    MapWaypointLabel_t *E = &MapWaypointLabelList[i];
    // draws if they are in task unconditionally,
    // otherwise, does comparison
    if (!E->inTask && !E->isAirport && E->isLandable)
      TextInBox(canvas, E->Name, E->Pos.x, E->Pos.y, E->Mode, MapRect, &label_block);
  }

  // now draw normal waypoints in order of range (furthest away last)
  for (unsigned i = 0; i < MapWaypointLabelListCount; i++) {
    MapWaypointLabel_t *E = &MapWaypointLabelList[i];
    if (!E->inTask && !E->isAirport && !E->isLandable)
      TextInBox(canvas, E->Name, E->Pos.x, E->Pos.y, E->Mode, MapRect, &label_block);
  }
}

void
MapWaypointLabelClear()
{
  MapWaypointLabelListCount = 0;
}
