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

#include "Replay/NmeaReplay.hpp"
#include "IO/FileLineReader.hpp"

#include <algorithm>

#include "Navigation/GeoPoint.hpp"
#include "StringUtil.hpp"

NmeaReplay::NmeaReplay() :
  TimeScale(1.0),
  Enabled(false),
  reader(NULL)
{
  FileName[0] = _T('\0');
}

void
NmeaReplay::Stop()
{
  CloseFile();

  Enabled = false;
}

void
NmeaReplay::Start()
{
  if (Enabled)
    Stop();

  if (!OpenFile()) {
    on_bad_file();
    return;
  }

  Enabled = true;
}

const TCHAR*
NmeaReplay::GetFilename()
{
  return FileName;
}

void
NmeaReplay::SetFilename(const TCHAR *name)
{
  if (!name || string_is_empty(name))
    return;

  if (_tcscmp(FileName, name) != 0)
    _tcscpy(FileName, name);
}

bool
NmeaReplay::ReadUntilRMC(bool ignore)
{
  char *buffer;

  while ((buffer = reader->read()) != NULL) {
    if (!ignore)
      on_sentence(buffer);

    if (strstr(buffer, "$GPRMC") == buffer)
      return true;
  }

  return false;
}

bool
NmeaReplay::Update()
{
  if (!Enabled)
    return false;

  if (!update_time())
    return true;

  for (fixed i = fixed_one; i <= TimeScale; i += fixed_one) {
    Enabled = ReadUntilRMC(i != TimeScale);
    if (!Enabled)
      return false;
  }

  if (!Enabled) {
    Stop();
  }

  return Enabled;
}

bool
NmeaReplay::OpenFile()
{
  if (reader)
    return true;

  if (string_is_empty(FileName))
    return false;

  reader = new FileLineReaderA(FileName);
  if (reader->error()) {
    CloseFile();
    return false;
  }

  return true;
}

void
NmeaReplay::CloseFile()
{
  delete reader;
  reader = NULL;
}

bool
NmeaReplay::IsEnabled()
{
  return Enabled;
}

bool
NmeaReplay::update_time()
{
  return true;
}
