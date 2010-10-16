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

#include "Hardware/AltairControl.hpp"

#include <tchar.h>

enum {
  IOCTL_TRA_BACKLIGHTSETVALUE = 5000,
  IOCTL_TRA_BACKLIGHTGETVALUE = 5001,
  IOCTL_TRA_GETINFO = 5030,
  IOCTL_TRA_SHORTBEEP = 5060,
};

AltairControl::AltairControl()
 :handle(::CreateFile(_T("TRA1:"), GENERIC_READ|GENERIC_WRITE, 0,
                      NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))
{
}

AltairControl::~AltairControl()
{
  if (handle != INVALID_HANDLE_VALUE)
    ::CloseHandle(handle);
}

bool
AltairControl::ShortBeep()
{
  if (handle == INVALID_HANDLE_VALUE)
    return false;

  return ::DeviceIoControl(handle, IOCTL_TRA_SHORTBEEP,
                           NULL, 0, NULL, 0, NULL, NULL) != 0;
}

bool
AltairControl::GetBacklight(int &value_r)
{
  if (handle == INVALID_HANDLE_VALUE)
    return false;

  return ::DeviceIoControl(handle, IOCTL_TRA_BACKLIGHTGETVALUE,
                           &value_r, sizeof(value_r),
                           NULL, 0, NULL, NULL) != 0;
}

bool
AltairControl::SetBacklight(int value)
{
  if (handle == INVALID_HANDLE_VALUE)
    return false;

  return ::DeviceIoControl(handle, IOCTL_TRA_BACKLIGHTSETVALUE,
                           &value, sizeof(value), NULL, 0, NULL, NULL) != 0;
}
