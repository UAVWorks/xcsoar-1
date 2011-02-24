/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2011 The XCSoar Project
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

#ifndef XCSOAR_SCREEN_GDI_KEY_H
#define XCSOAR_SCREEN_GDI_KEY_H

#if defined(__WINE__) || defined(__CYGWIN__)
/* the WINE 1.1.32 headers are broken, the definition of
   LPSECURITY_ATTRIBUTES is missing in winuser.h */
#include <windows.h>
#endif

#include <winuser.h>

#ifndef _WIN32_WCE
enum {
  VK_APP1 = '1',
  VK_APP2 = '2',
  VK_APP3 = '3',
  VK_APP4 = '4',
  VK_APP5 = '5',
  VK_APP6 = '6',
};
#endif

#endif
