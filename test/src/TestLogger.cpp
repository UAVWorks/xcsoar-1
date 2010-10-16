/* Copyright_License {

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

#include "Logger/IGCWriter.hpp"
#include "OS/FileUtil.hpp"
#include "NMEA/Info.hpp"
#include "IO/FileLineReader.hpp"
#include "TestUtil.hpp"

#include <assert.h>

static void
CheckTextFile(const TCHAR *path, const char *const* expect)
{
  FileLineReaderA reader(path);
  ok1(!reader.error());

  const char *line;
  while ((line = reader.read()) != NULL) {
    if (*line == 'G')
      break;

    ok1(*expect != NULL);

    if (strncmp(*expect, "HFFTYFR TYPE:", 13) == 0) {
      ok1(strncmp(line, "HFFTYFR TYPE:", 13) == 0);
    } else {
      ok1(strcmp(line, *expect) == 0);
    }

    ++expect;
  }

  ok1(*expect == NULL);
}

static const char *const expect[] = {
  "AXCSfoo",
  "HFDTE040910",
  "HFFXA50  ",
  "HFPLTPILOT:Pilot Name",
  "HFGTYGLIDERTYPE:ASK-21",
  "HFGIDGLIDERID:D-1234",
  "HFFTYFR TYPE:XCSOAR XCSOAR",
  "HFGPS: bar",
  "HFDTM100Datum: WGS-84",
  "I023638FXA3940SIU  ",
  "C040910112233000000000001",
  "C0000000N00000000ETAKEOFF",
  "C5103117N00742367EBERGNEUSTADT",
  "C5037933N01043567ESUHL",
  "C5103117N00742367EBERGNEUSTADT",
  "C0000000N00000000ELANDING",
  "B1122385103117N00742367EA004900048700000",
  "E112243my_event",
  "B1122435103117N00742367EA004900048700000",
  "LPLT",
  "B1122535103117S00742367WA004900048700000",
  NULL
};

int main(int argc, char **argv)
{
  plan_tests(45);

  const TCHAR *path = _T("output/test/test.igc");
  File::Delete(path);

  static const GeoPoint home(Angle::degrees(fixed(7.7061111111111114)),
                             Angle::degrees(fixed(51.051944444444445)));
  static const GeoPoint tp(Angle::degrees(fixed(10.726111111111111)),
                           Angle::degrees(fixed(50.632222222222225)));

  static NMEA_INFO i;
  i.DateTime.year = 2010;
  i.DateTime.month = 9;
  i.DateTime.day = 4;
  i.DateTime.hour = 11;
  i.DateTime.minute = 22;
  i.DateTime.second = 33;
  i.Location = home;
  i.GPSAltitude = fixed(487);
  i.BaroAltitude = fixed(490);

  IGCWriter writer(path, i);

  writer.header(i.DateTime, _T("Pilot Name"), _T("ASK-21"), _T("D-1234"),
                _T("foo"), _T("bar"));
  writer.StartDeclaration(i.DateTime, 3);
  writer.AddDeclaration(home, _T("Bergneustadt"));
  writer.AddDeclaration(tp, _T("Suhl"));
  writer.AddDeclaration(home, _T("Bergneustadt"));
  writer.EndDeclaration();

  i.DateTime.second += 5;
  writer.LogPoint(i);
  i.DateTime.second += 5;
  writer.LogEvent(i, "my_event");
  i.DateTime.second += 5;
  writer.LoggerNote(_T("my_note"));

  i.DateTime.second += 5;
  i.Location = GeoPoint(Angle::degrees(fixed(-7.7061111111111114)),
                        Angle::degrees(fixed(-51.051944444444445)));
  writer.LogPoint(i);

  writer.finish(i);
  writer.sign();

  CheckTextFile(path, expect);

  GRecord grecord;
  grecord.Init();
  grecord.SetFileName(path);
  ok1(grecord.VerifyGRecordInFile());

  return 0;
}
