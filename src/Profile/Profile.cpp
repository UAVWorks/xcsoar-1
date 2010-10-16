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

#include "Profile/Profile.hpp"
#include "Profile/Writer.hpp"
#include "LogFile.hpp"
#include "Asset.hpp"
#include "LocalPath.hpp"
#include "StringUtil.hpp"
#include "IO/FileLineReader.hpp"
#include "IO/TextWriter.hpp"

#include <string.h>

#define XCSPROFILE "xcsoar-registry.prf"

TCHAR startProfileFile[MAX_PATH];
TCHAR defaultProfileFile[MAX_PATH];

void
Profile::Load()
{
  if (!use_files())
    return;

  LogStartUp(_T("Loading profiles"));
  // load registry backup if it exists
  LoadFile(defaultProfileFile);

  if (_tcscmp(startProfileFile, defaultProfileFile) != 0)
    LoadFile(startProfileFile);
}

void
Profile::LoadFile(const TCHAR *szFile)
{
  if (!use_files())
    return;

  if (string_is_empty(szFile))
    return;

  FileLineReader reader(szFile);
  if (reader.error())
    return;

  LogStartUp(_T("Loading profile from %s"), szFile);

  TCHAR *line;
  while ((line = reader.read()) != NULL) {
    if (string_is_empty(line) || *line == _T('#'))
      continue;

    TCHAR *p = _tcschr(line, _T('='));
    if (p == line || p == NULL)
      continue;

    *p = _T('\0');
    TCHAR *value = p + 1;

#ifdef PROFILE_KEY_PREFIX
    TCHAR key[sizeof(PROFILE_KEY_PREFIX) + _tcslen(line)];
    _tcscpy(key, PROFILE_KEY_PREFIX);
    _tcscat(key, line);
#else
    const TCHAR *key = line;
#endif

    if (*value == _T('"')) {
      ++value;
      p = _tcschr(value, _T('"'));
      if (p == NULL)
        continue;

      *p = _T('\0');

      Set(key, value);
    } else {
      long l = _tcstol(value, &p, 10);
      if (p > value)
        Set(key, l);
    }
  }
}

void
Profile::Save()
{
  if (!use_files())
    return;

  LogStartUp(_T("Saving profiles"));
  SaveFile(defaultProfileFile);
}

void
Profile::SaveFile(const TCHAR *szFile)
{
  if (!use_files())
    return;

  if (string_is_empty(szFile))
    return;

  // Try to open the file for writing
  TextWriter writer(szFile);
  // ... on error -> return
  if (writer.error())
    return;

  ProfileWriter profile_writer(writer);

  LogStartUp(_T("Saving profile to %s"), szFile);
  Export(profile_writer);
}


void
Profile::SetFiles(const TCHAR* override)
{
  if (!use_files())
    return;

  // Set the default profile file
  if (is_altair())
    LocalPath(defaultProfileFile, _T("config/")_T(XCSPROFILE));
  else
    LocalPath(defaultProfileFile, _T(XCSPROFILE));

  // Set the profile file to load at startup
  // -> to the default file
  _tcscpy(startProfileFile, defaultProfileFile);

  // -> to the given filename (if exists)
  if (!string_is_empty(override))
    _tcsncpy(startProfileFile, override, MAX_PATH - 1);
}

bool
Profile::GetPath(const TCHAR *key, TCHAR *value)
{
  if (!Get(key, value, MAX_PATH) || string_is_empty(value))
    return false;

  ExpandLocalPath(value);
  return true;
}
