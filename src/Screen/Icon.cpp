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

#include "Screen/Icon.hpp"
#include "Screen/Layout.hpp"
#include "Screen/Canvas.hpp"

void
MaskedIcon::load_big(unsigned id, unsigned big_id, bool center)
{
  if (Layout::ScaleEnabled()) {
    if (big_id > 0)
      bitmap.load(big_id);
    else
      bitmap.load_stretch(id, Layout::FastScale(1));
  } else
    bitmap.load(id);

  assert(defined());

  size = bitmap.get_size();
  /* left half is mask, right half is icon */
  size.cx /= 2;

  if (center) {
    origin.x = size.cx / 2;
    origin.y = size.cy / 2;
  } else {
    origin.x = 0;
    origin.y = 0;
  }
}

void
MaskedIcon::draw(Canvas &canvas, int x, int y) const
{
  assert(defined());

  canvas.copy_or(x - origin.x, y - origin.y, size.cx, size.cy,
                 bitmap, 0, 0);
  canvas.copy_and(x - origin.x, y - origin.y, size.cx, size.cy,
                  bitmap, size.cx, 0);
}
