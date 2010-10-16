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

#include "DataField/Integer.hpp"
#include "Math/FastMath.h"
#include "Compatibility/string.h"
#include "Asset.hpp"

#include <stdlib.h>
#include <stdio.h>

static bool DataFieldKeyUp = false;

bool
DataFieldInteger::GetAsBoolean(void) const
{
  return (mValue != 0);
}

int
DataFieldInteger::GetAsInteger(void) const
{
  return mValue;
}

fixed
DataFieldInteger::GetAsFixed() const
{
  return fixed(mValue);
}

const TCHAR *
DataFieldInteger::GetAsString(void) const
{
  TCHAR *mOutBuf = const_cast<TCHAR *>(this->mOutBuf);
  _stprintf(mOutBuf, mEditFormat, mValue);
  return mOutBuf;
}

const TCHAR *
DataFieldInteger::GetAsDisplayString(void) const
{
  TCHAR *mOutBuf = const_cast<TCHAR *>(this->mOutBuf);
  _stprintf(mOutBuf, mDisplayFormat, mValue, mUnits);
  return mOutBuf;
}

void
DataFieldInteger::Set(int Value)
{
  mValue = Value;
}

void
DataFieldInteger::SetAsBoolean(bool Value)
{
  if (Value)
    SetAsInteger(1);
  else
    SetAsInteger(0);
}

void
DataFieldInteger::SetAsInteger(int Value)
{
  if (Value < mMin)
    Value = mMin;
  if (Value > mMax)
    Value = mMax;
  if (mValue != Value) {
    mValue = Value;
    if (!GetDetachGUI())
      (mOnDataAccess)(this, daChange);
  }
}

void
DataFieldInteger::SetAsFloat(fixed Value)
{
  SetAsInteger(iround(Value));
}

void
DataFieldInteger::SetAsString(const TCHAR *Value)
{
  SetAsInteger(_ttoi(Value));
}

void
DataFieldInteger::Inc(void)
{
  SetAsInteger(mValue + mStep * SpeedUp(true));
}

void
DataFieldInteger::Dec(void)
{
  SetAsInteger(mValue - mStep * SpeedUp(false));
}

int
DataFieldInteger::SpeedUp(bool keyup)
{
  int res = 1;

  if (is_altair())
    return res;

  if (GetDisableSpeedUp() == true)
    return 1;

  if (keyup != DataFieldKeyUp) {
    mSpeedup = 0;
    DataFieldKeyUp = keyup;
    last_step.update();
    return 1;
  }

  if (!last_step.check(200)) {
    mSpeedup++;
    if (mSpeedup > 5) {
      res = 10;
      last_step.update_offset(350);
      return (res);
    }
  } else
    mSpeedup = 0;

  last_step.update();

  return res;
}

ComboList *
DataFieldInteger::CreateComboList() const
{
  DataFieldInteger clone(*this);
  return clone.CreateComboListStepping();
}
