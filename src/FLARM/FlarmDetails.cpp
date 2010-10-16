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

#include "FLARM/FlarmDetails.hpp"
#include "Util/StringUtil.hpp"
#include "LogFile.hpp"
#include "LocalPath.hpp"
#include "Sizes.h"
#include "FLARM/FLARMNet.hpp"
#include "IO/DataFile.hpp"
#include "IO/TextWriter.hpp"

#include <stdlib.h>

static FLARMNetDatabase flarm_net;

static int NumberOfFLARMNames = 0;

typedef struct {
  FlarmId ID;
  TCHAR Name[21];
} FLARM_Names_t;

#define MAXFLARMNAMES 200

static FLARM_Names_t FLARM_Names[MAXFLARMNAMES];

void
FlarmDetails::Load()
{
  LogStartUp(_T("FlarmDetails::Load"));

  LoadSecondary();
  LoadFLARMnet();
}

void
FlarmDetails::LoadFLARMnet()
{
  NLineReader *reader = OpenDataTextFileA(_T("data.fln"));
  if (reader == NULL)
    return;

  unsigned num_records = flarm_net.LoadFile(*reader);
  delete reader;

  if (num_records > 0)
    LogStartUp(_T("%u FLARMnet ids found"), num_records);
}

void
FlarmDetails::Reset()
{
  NumberOfFLARMNames = 0;
}

static void
LoadSecondaryFile(TLineReader &reader)
{
  TCHAR *line;
  while ((line = reader.read()) != NULL) {
    TCHAR *endptr;
    FlarmId id;
    id.parse(line, &endptr);
    if (endptr > line && endptr[0] == _T('=') && endptr[1] != _T('\0')) {
      TCHAR *Name = endptr + 1;
      TrimRight(Name);
      if (!FlarmDetails::AddSecondaryItem(id, Name, false))
        break; // cant add anymore items !
    }
  }
}

void
FlarmDetails::LoadSecondary()
{
  LogStartUp(_T("OpenFLARMDetails"));

  // if (FLARM Details already there) delete them;
  if (NumberOfFLARMNames)
    Reset();

  TLineReader *reader = OpenDataTextFile(_T("xcsoar-flarm.txt"));
  if (reader != NULL) {
    LoadSecondaryFile(*reader);
    delete reader;
  }
}

void
FlarmDetails::SaveSecondary()
{
  TextWriter *writer = CreateDataTextFile(_T("xcsoar-flarm.txt"));
  if (writer == NULL)
    return;

  TCHAR id[16];
  for (int z = 0; z < NumberOfFLARMNames; z++)
    writer->printfln(_T("%s=%s"),
                     FLARM_Names[z].ID.format(id), FLARM_Names[z].Name);

  delete writer;
}

int
FlarmDetails::LookupSecondaryIndex(FlarmId id)
{
  for (int i = 0; i < NumberOfFLARMNames; i++)
    if (FLARM_Names[i].ID == id)
      return i;

  return -1;
}

int
FlarmDetails::LookupSecondaryIndex(const TCHAR *cn)
{
  for (int i = 0; i < NumberOfFLARMNames; i++)
    if (_tcscmp(FLARM_Names[i].Name, cn) == 0)
      return i;

  return -1;
}

const FLARMNetRecord *
FlarmDetails::LookupRecord(FlarmId id)
{
  // try to find flarm from FLARMNet.org File
  const FLARMNetRecord *record = flarm_net.Find(id);
  if (record != NULL)
    return record;

  return NULL;
}

const TCHAR *
FlarmDetails::LookupCallsign(FlarmId id)
{
  // try to find flarm from userFile
  int index = LookupSecondaryIndex(id);
  if (index != -1)
    return FLARM_Names[index].Name;

  // try to find flarm from FLARMNet.org File
  const FLARMNetRecord *record = flarm_net.Find(id);
  if (record != NULL)
    return record->cn;

  return NULL;
}

FlarmId
FlarmDetails::LookupId(const TCHAR *cn)
{
  // try to find flarm from userFile
  int index = LookupSecondaryIndex(cn);
  if (index != -1)
    return FLARM_Names[index].ID;

  // try to find flarm from FLARMNet.org File
  const FLARMNetRecord *record = flarm_net.Find(cn);
  if (record != NULL)
    return record->GetId();

  FlarmId id;
  id.clear();
  return id;
}

bool
FlarmDetails::AddSecondaryItem(FlarmId id, const TCHAR *name, bool saveFile)
{
  int index = LookupSecondaryIndex(id);
  if (index != -1) {
    // modify existing record
    FLARM_Names[index].ID = id;
    _tcsncpy(FLARM_Names[index].Name, name, 20);
    FLARM_Names[index].Name[20] = 0;
    if (saveFile)
      SaveSecondary();

    return true;
  }

  if (NumberOfFLARMNames >= MAXFLARMNAMES - 1)
    return false;

  // create new record
  FLARM_Names[NumberOfFLARMNames].ID = id;
  _tcsncpy(FLARM_Names[NumberOfFLARMNames].Name, name, 20);
  FLARM_Names[NumberOfFLARMNames].Name[20] = 0;
  NumberOfFLARMNames++;
  if (saveFile)
    SaveSecondary();

  return true;
}
