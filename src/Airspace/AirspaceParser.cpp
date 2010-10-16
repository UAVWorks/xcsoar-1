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

#include "AirspaceParser.hpp"
#include "Airspace/Airspaces.hpp"
#include "ProgressGlue.hpp"
#include "Units.hpp"
#include "Dialogs/Message.hpp"
#include "Language.hpp"
#include "Util/StringUtil.hpp"
#include "Math/Earth.hpp"
#include "IO/LineReader.hpp"
#include "Airspace/AirspacePolygon.hpp"
#include "Airspace/AirspaceCircle.hpp"
#include "Compatibility/string.h"

#include <math.h>
#include <tchar.h>
#include <ctype.h>
#include <assert.h>

enum asFileType {
  ftUnknown,
  ftOpenAir,
  ftTNP
};

static const int k_nAreaCount = 12;
static const TCHAR* k_strAreaStart[k_nAreaCount] = {
  _T("R"),
  _T("Q"),
  _T("P"),
  _T("CTR"),
  _T("A"),
  _T("B"),
  _T("C"),
  _T("D"),
  _T("GP"),
  _T("W"),
  _T("E"),
  _T("F")
};

static const int k_nAreaType[k_nAreaCount] = {
  RESTRICT,
  DANGER,
  PROHIBITED,
  CTR,
  CLASSA,
  CLASSB,
  CLASSC,
  CLASSD,
  NOGLIDER,
  WAVE,
  CLASSE,
  CLASSF
};

// this can now be called multiple times to load several airspaces.

struct TempAirspaceType
{
  TempAirspaceType() {
    reset();
  }

  bool Waiting;

  // General
  tstring Name;
  AirspaceClass_t Type;
  AIRSPACE_ALT Base;
  AIRSPACE_ALT Top;

  // Polygon
  std::vector<GeoPoint> points;

  // Circle or Arc
  GeoPoint Center;
  fixed Radius;

  // Arc
  int Rotation;

  void
  reset()
  {
    Type = OTHER;
    points.clear();
    Center.Longitude = Angle::native(fixed_zero);
    Center.Latitude = Angle::native(fixed_zero);
    Rotation = 1;
    Radius = fixed_zero;
    Waiting = true;
  }

  void
  AddPolygon(Airspaces &airspace_database)
  {
    AbstractAirspace *as = new AirspacePolygon(points);
    as->set_properties(Name, Type, Base, Top);
    airspace_database.insert(as);
  }

  void
  AddCircle(Airspaces &airspace_database)
  {
    AbstractAirspace *as = new AirspaceCircle(Center, Radius);
    as->set_properties(Name, Type, Base, Top);
    airspace_database.insert(as);
  }
};

static bool
ShowParseWarning(int line, const TCHAR* str)
{
  TCHAR sTmp[MAX_PATH];
  _stprintf(sTmp, _T("%s: %d\r\n\"%s\"\r\n%s."),
            _("Parse Error at Line"), line, str,
            _("Line skipped."));
  return (MessageBoxX(sTmp, _("Airspace"), MB_OKCANCEL) == IDOK);

}

static void
ReadAltitude(const TCHAR *Text_, AIRSPACE_ALT *Alt)
{
  TCHAR Text[128];
  bool fHasUnit = false;

  _tcsncpy(Text, Text_, sizeof(Text) / sizeof(Text[0]));
  Text[sizeof(Text) / sizeof(Text[0]) - 1] = '\0';

  _tcsupr(Text);

  Alt->Altitude = fixed_zero;
  Alt->FL = fixed_zero;
  Alt->AGL = fixed_zero;
  Alt->Base = abUndef;

  const TCHAR *p = Text;
  while (true) {
    while (*p == _T(' '))
      ++p;

    if (_istdigit(*p)) {
      TCHAR *endptr;
      fixed d = fixed(_tcstod(p, &endptr));

      if (Alt->Base == abFL)
        Alt->FL = d;
      else if (Alt->Base == abAGL)
        Alt->AGL = d;
      else
        Alt->Altitude = d;

      p = endptr;
    } else if (_tcsncmp(p, _T("GND"), 3) == 0) {
      // JMW support XXXGND as valid, equivalent to XXXAGL
      Alt->Base = abAGL;
      if (Alt->Altitude > fixed_zero) {
        Alt->AGL = Alt->Altitude;
        Alt->Altitude = fixed_zero;
      } else {
        Alt->FL = fixed_zero;
        Alt->Altitude = fixed_zero;
        Alt->AGL = fixed_minus_one;
        fHasUnit = true;
      }

      p += 3;
    } else if (_tcsncmp(p, _T("SFC"), 3) == 0) {
      Alt->Base = abAGL;
      Alt->FL = fixed_zero;
      Alt->Altitude = fixed_zero;
      Alt->AGL = fixed_minus_one;
      fHasUnit = true;

      p += 3;
    } else if (_tcsncmp(p, _T("FL"), 2) == 0) {
      // this parses "FL=150" and "FL150"
      Alt->Base = abFL;
      fHasUnit = true;

      p += 2;
    } else if (*p == _T('F')) {
      Alt->Altitude = Units::ToSysUnit(Alt->Altitude, unFeet);
      fHasUnit = true;

      ++p;
      if (*p == _T('T'))
        ++p;
    } else if (_tcsncmp(p, _T("MSL"), 3) == 0) {
      Alt->Base = abMSL;

      p += 3;
    } else if (*p == _T('M')) {
      // JMW must scan for MSL before scanning for M
      fHasUnit = true;

      ++p;
    } else if (_tcsncmp(p, _T("AGL"), 3) == 0) {
      Alt->Base = abAGL;
      Alt->AGL = Alt->Altitude;
      Alt->Altitude = fixed_zero;

      p += 3;
    } else if (_tcsncmp(p, _T("STD"), 3) == 0) {
      if (Alt->Base != abUndef) {
        // warning! multiple base tags
      }
      Alt->Base = abFL;
      Alt->FL = Units::ToUserUnit(Alt->Altitude, unFlightLevel);

      p += 3;
    } else if (_tcsncmp(p, _T("UNL"), 3) == 0) {
      // JMW added Unlimited (used by WGC2008)
      Alt->Base = abMSL;
      Alt->AGL = fixed_minus_one;
      Alt->Altitude = fixed(50000);

      p += 3;
    } else if (*p == _T('\0'))
      break;
    else
      ++p;
  }

  if (!fHasUnit && (Alt->Base != abFL)) {
    // ToDo warning! no unit defined use feet or user alt unit
    // Alt->Altitude = Units::ToSysAltitude(Alt->Altitude);
    Alt->Altitude = Units::ToSysUnit(Alt->Altitude, unFeet);
    Alt->AGL = Units::ToSysUnit(Alt->AGL, unFeet);
  }

  if (Alt->Base == abUndef)
    // ToDo warning! no base defined use MSL
    Alt->Base = abMSL;
}

static bool
ReadCoords(const TCHAR *Text, GeoPoint &point)
{
  // Format: 53:20:41 N 010:24:41 E

  TCHAR *Stop;

  // ToDo, add more error checking and making it more tolerant/robust

  long deg = _tcstol(Text, &Stop, 10);
  if ((Text == Stop) || (*Stop == '\0'))
    return false;

  Stop++;
  long min = _tcstol(Stop, &Stop, 10);
  if (*Stop == '\0')
    return false;

  long sec = 0;
  if (*Stop == ':') {
    Stop++;
    if (*Stop == '\0')
      return false;

    sec = _tcstol(Stop, &Stop, 10);
    if (sec < 0 || sec >= 60) {
      // ToDo
    }
  }

  point.Latitude = Angle::dms(fixed(deg), fixed(min), fixed(sec));

  if (*Stop == ' ')
    Stop++;

  if (*Stop == '\0')
    return false;

  if ((*Stop == 'S') || (*Stop == 's'))
    point.Latitude.flip();

  Stop++;
  if (*Stop == '\0')
    return false;

  deg = _tcstol(Stop, &Stop, 10);
  Stop++;
  min = _tcstol(Stop, &Stop, 10);
  if (*Stop == ':') {
    Stop++;
    if (*Stop == '\0')
      return false;

    sec = _tcstol(Stop, &Stop, 10);
  }

  point.Longitude = Angle::dms(fixed(deg), fixed(min), fixed(sec));

  if (*Stop == ' ')
    Stop++;

  if (*Stop == '\0')
    return false;

  if ((*Stop == 'W') || (*Stop == 'w'))
    point.Longitude.flip();
  point.Longitude = point.Longitude.as_bearing();
  return true;
}

static void
CalculateSector(const TCHAR *Text, TempAirspaceType &temp_area)
{
  fixed Radius;
  TCHAR *Stop;
  GeoPoint TempPoint;
  static const fixed fixed_75 = fixed(7.5);
  const Angle BearingStep = Angle::degrees(temp_area.Rotation * fixed(5));

  Radius = Units::ToSysUnit(fixed(_tcstod(&Text[2], &Stop)), unNauticalMiles);
  Angle StartBearing = Angle::degrees(fixed(_tcstod(&Stop[1], &Stop)));
  Angle EndBearing = Angle::degrees(fixed(_tcstod(&Stop[1], &Stop)));

  if (EndBearing < StartBearing)
    EndBearing += Angle::degrees(fixed_360);

  while ((EndBearing - StartBearing).magnitude_degrees() > fixed_75) {
    StartBearing = StartBearing.as_bearing();
    FindLatitudeLongitude(temp_area.Center, StartBearing, Radius, &TempPoint);
    temp_area.points.push_back(TempPoint);
    StartBearing += BearingStep;
  }

  FindLatitudeLongitude(temp_area.Center, EndBearing, Radius, &TempPoint);
  temp_area.points.push_back(TempPoint);
}

static void
CalculateArc(const TCHAR *Text, TempAirspaceType &temp_area)
{
  GeoPoint Start;
  GeoPoint End;
  Angle StartBearing;
  fixed Radius;
  const TCHAR *Comma = NULL;
  GeoPoint TempPoint;
  static const fixed fixed_75 = fixed(7.5);
  const Angle BearingStep = Angle::degrees(temp_area.Rotation * fixed(5));

  ReadCoords(&Text[3], Start);

  Comma = _tcschr(Text, ',');
  if (!Comma)
    return;

  ReadCoords(&Comma[1], End);

  DistanceBearing(temp_area.Center, Start, &Radius, &StartBearing);
  Angle EndBearing = Bearing(temp_area.Center, End);
  TempPoint.Latitude = Start.Latitude;
  TempPoint.Longitude = Start.Longitude;
  temp_area.points.push_back(TempPoint);

  while ((EndBearing - StartBearing).magnitude_degrees() > fixed_75) {
    StartBearing += BearingStep;
    StartBearing = StartBearing.as_bearing();
    FindLatitudeLongitude(temp_area.Center, StartBearing, Radius, &TempPoint);
    temp_area.points.push_back(TempPoint);
  }

  TempPoint = End;
  temp_area.points.push_back(TempPoint);
}

static AirspaceClass_t
ParseType(const TCHAR* text)
{
  for (int i = 0; i < k_nAreaCount; i++)
    if (string_after_prefix(text, k_strAreaStart[i]))
      return (AirspaceClass_t)k_nAreaType[i];

  return OTHER;
}

/**
 * Returns the value of the specified line, after a space character
 * which is skipped.  If the input is empty (without a leading space),
 * an empty string is returned, as a special case to work around
 * broken input files.
 *
 * @return the first character of the value, or NULL if the input is
 * malformed
 */
static const TCHAR *
value_after_space(const TCHAR *p)
{
  if (string_is_empty(p))
    return p;

  if (*p != _T(' '))
    /* not a space: must be a malformed line */
    return NULL;

  /* skip the space */
  return p + 1;
}

static bool
ParseLine(Airspaces &airspace_database, const TCHAR *line,
          TempAirspaceType &temp_area)
{
  const TCHAR *value;

  // Only return expected lines
  switch (line[0]) {
  case _T('A'):
  case _T('a'):
    switch (line[1]) {
    case _T('C'):
    case _T('c'):
      value = value_after_space(line + 2);
      if (value == NULL)
        break;

      if (!temp_area.Waiting)
        temp_area.AddPolygon(airspace_database);

      temp_area.reset();

      temp_area.Type = ParseType(value);
      temp_area.Waiting = false;
      break;

    case _T('N'):
    case _T('n'):
      value = value_after_space(line + 2);
      if (value != NULL)
        temp_area.Name = value;
      break;

    case _T('L'):
    case _T('l'):
      value = value_after_space(line + 2);
      if (value != NULL)
        ReadAltitude(value, &temp_area.Base);
      break;

    case _T('H'):
    case _T('h'):
      value = value_after_space(line + 2);
      if (value != NULL)
        ReadAltitude(value, &temp_area.Top);
      break;

    default:
      return true;
    }

    break;

  case _T('D'):
  case _T('d'):
    switch (line[1]) {
    case _T('A'):
    case _T('a'):
      CalculateSector(line, temp_area);
      break;

    case _T('B'):
    case _T('b'):
      CalculateArc(line, temp_area);
      break;

    case _T('C'):
    case _T('c'):
      temp_area.Radius = Units::ToSysUnit(fixed(_tcstod(&line[2], NULL)),
                                          unNauticalMiles);
      temp_area.AddCircle(airspace_database);
      temp_area.reset();
      break;

    case _T('P'):
    case _T('p'):
      value = value_after_space(line + 2);
      if (value == NULL)
        break;

    {
      GeoPoint TempPoint;

      if (!ReadCoords(value, TempPoint))
        return false;

      temp_area.points.push_back(TempPoint);
      break;
    }
    default:
      return true;
    }

    break;

  case _T('V'):
  case _T('v'):
    // Need to set these while in count mode, or DB/DA will crash
    if (string_after_prefix_ci(&line[2], _T("X="))) {
      if (!ReadCoords(&line[4],temp_area.Center))
        return false;
    } else if (string_after_prefix_ci(&line[2], _T("D=-"))) {
      temp_area.Rotation = -1;
    } else if (string_after_prefix_ci(&line[2], _T("D=+"))) {
      temp_area.Rotation = +1;
    }
  }

  return true;
}

static AirspaceClass_t
ParseClassTNP(const TCHAR* text)
{
  if (text[0] == _T('A'))
    return CLASSA;

  if (text[0] == _T('B'))
    return CLASSB;

  if (text[0] == _T('C'))
    return CLASSC;

  if (text[0] == _T('D'))
    return CLASSD;

  if (text[0] == _T('E'))
    return CLASSE;

  if (text[0] == _T('F'))
    return CLASSF;

  return OTHER;
}

static AirspaceClass_t
ParseTypeTNP(const TCHAR* text)
{
  if (_tcsicmp(text, _T("C")) == 0 ||
      _tcsicmp(text, _T("CTA")) == 0 ||
      _tcsicmp(text, _T("CTA")) == 0 ||
      _tcsicmp(text, _T("CTA/CTR")) == 0)
    return CTR;

  if (_tcsicmp(text, _T("R")) == 0 ||
      _tcsicmp(text, _T("RESTRICTED")) == 0)
    return RESTRICT;

  if (_tcsicmp(text, _T("P")) == 0 ||
      _tcsicmp(text, _T("PROHIBITED")) == 0)
    return RESTRICT;

  if (_tcsicmp(text, _T("D")) == 0 ||
      _tcsicmp(text, _T("DANGER")) == 0)
    return RESTRICT;

  if (_tcsicmp(text, _T("G")) == 0 ||
      _tcsicmp(text, _T("GSEC")) == 0)
    return WAVE;

  return OTHER;
}

static bool
ParseCoordsTNP(const TCHAR *Text, GeoPoint &point)
{
  // Format: N542500 E0105000
  bool negative = false;
  long deg = 0, min = 0, sec = 0;
  TCHAR *ptr;

  if (Text[0] == _T('S') || Text[0] == _T('s'))
    negative = true;

  sec = _tcstol(&Text[1], &ptr, 10);
  deg = labs(sec / 10000);
  min = labs((sec - deg * 10000) / 100);
  sec = sec - min * 100 - deg * 10000;

  point.Latitude = Angle::dms(fixed(deg), fixed(min), fixed(sec));
  if (negative)
    point.Latitude.flip();

  negative = false;

  if (ptr[0] == _T(' '))
    ptr++;

  if (ptr[0] == _T('W') || ptr[0] == _T('w'))
    negative = true;

  sec = _tcstol(&ptr[1], &ptr, 10);
  deg = labs(sec / 10000);
  min = labs((sec - deg * 10000) / 100);
  sec = sec - min * 100 - deg * 10000;

  point.Longitude = Angle::dms(fixed(deg), fixed(min), fixed(sec));
  if (negative)
    point.Longitude.flip();

  point.Longitude = point.Longitude.as_bearing();

  return true;
}

static bool
ParseArcTNP(const TCHAR *Text, TempAirspaceType &temp_area)
{
  if (temp_area.points.empty())
    return false;

  // (ANTI-)CLOCKWISE RADIUS=34.95 CENTRE=N523333 E0131603 TO=N522052 E0122236

  GeoPoint from = temp_area.points.back();

  const TCHAR* parameter;
  if ((parameter = _tcsstr(Text, _T(" "))) == NULL)
    return false;
  if ((parameter = string_after_prefix_ci(parameter, _T(" CENTRE="))) == NULL)
    return false;
  ParseCoordsTNP(parameter, temp_area.Center);

  GeoPoint to;
  if ((parameter = _tcsstr(parameter, _T(" "))) == NULL)
    return false;
  parameter++;
  if ((parameter = _tcsstr(parameter, _T(" "))) == NULL)
    return false;
  if ((parameter = string_after_prefix_ci(parameter, _T(" TO="))) == NULL)
    return false;
  ParseCoordsTNP(parameter, to);

  Angle bearing_from;
  Angle bearing_to;
  fixed radius;

  static const fixed fixed_75 = fixed(7.5);
  const Angle BearingStep = Angle::degrees(temp_area.Rotation * fixed(5));

  DistanceBearing(temp_area.Center, from, &radius, &bearing_from);
  bearing_to = Bearing(temp_area.Center, to);

  GeoPoint TempPoint;
  while ((bearing_to - bearing_from).magnitude_degrees() > fixed_75) {
    bearing_from += BearingStep;
    bearing_from = bearing_from.as_bearing();
    FindLatitudeLongitude(temp_area.Center, bearing_from, radius, &TempPoint);
    temp_area.points.push_back(TempPoint);
  }

  return true;
}

static bool
ParseCircleTNP(const TCHAR *Text, TempAirspaceType &temp_area)
{
  // CIRCLE RADIUS=17.00 CENTRE=N533813 E0095943

  const TCHAR* parameter;
  if ((parameter = string_after_prefix_ci(Text, _T("RADIUS="))) == NULL)
    return false;
  temp_area.Radius = Units::ToSysUnit(fixed(_tcstod(parameter, NULL)),
                                      unNauticalMiles);

  if ((parameter = _tcsstr(parameter, _T(" "))) == NULL)
    return false;
  if ((parameter = string_after_prefix_ci(parameter, _T(" CENTRE="))) == NULL)
    return false;
  ParseCoordsTNP(parameter, temp_area.Center);

  return true;
}

static bool
ParseLineTNP(Airspaces &airspace_database, const TCHAR *line,
             TempAirspaceType &temp_area, bool &ignore)
{
  const TCHAR* parameter;
  if ((parameter = string_after_prefix_ci(line, _T("INCLUDE="))) != NULL) {
    if (_tcsicmp(parameter, _T("YES")) == 0)
      ignore = false;
    else if (_tcsicmp(parameter, _T("NO")) == 0)
      ignore = true;

    return true;
  }

  if (ignore)
    return true;

  if ((parameter = string_after_prefix_ci(line, _T("TITLE="))) != NULL) {
    temp_area.Name = parameter;
  } else if ((parameter = string_after_prefix_ci(line, _T("TYPE="))) != NULL) {
    if (!temp_area.Waiting)
      temp_area.AddPolygon(airspace_database);

    temp_area.reset();

    temp_area.Type = ParseTypeTNP(parameter);
    temp_area.Waiting = false;
  } else if ((parameter = string_after_prefix_ci(line, _T("CLASS="))) != NULL) {
    if (temp_area.Type == OTHER)
      temp_area.Type = ParseClassTNP(parameter);
  } else if ((parameter = string_after_prefix_ci(line, _T("TOPS="))) != NULL) {
    ReadAltitude(parameter, &temp_area.Top);
  } else if ((parameter = string_after_prefix_ci(line, _T("BASE="))) != NULL) {
    ReadAltitude(parameter, &temp_area.Base);
  } else if ((parameter = string_after_prefix_ci(line, _T("POINT="))) != NULL) {
    GeoPoint TempPoint;

    if (!ParseCoordsTNP(parameter, TempPoint))
      return false;

    temp_area.points.push_back(TempPoint);
  } else if ((parameter =
      string_after_prefix_ci(line, _T("CIRCLE "))) != NULL) {
    if (!ParseCircleTNP(parameter, temp_area))
      return false;

    temp_area.AddCircle(airspace_database);
  } else if ((parameter =
      string_after_prefix_ci(line, _T("CLOCKWISE "))) != NULL) {
    temp_area.Rotation = 1;
    if (!ParseArcTNP(parameter, temp_area))
      return false;
  } else if ((parameter =
      string_after_prefix_ci(line, _T("ANTI-CLOCKWISE "))) != NULL) {
    temp_area.Rotation = -1;
    if (!ParseArcTNP(parameter, temp_area))
      return false;
  }

  return true;
}

static asFileType
DetectFileType(const TCHAR *line)
{
  if (string_after_prefix_ci(line, _T("INCLUDE=")) ||
      string_after_prefix_ci(line, _T("TYPE=")))
    return ftTNP;

  const TCHAR *p = string_after_prefix_ci(line, _T("AC"));
  if (p != NULL && (string_is_empty(p) || *p == _T(' ')))
    return ftOpenAir;

  return ftUnknown;
}

bool
ReadAirspace(Airspaces &airspace_database, TLineReader &reader)
{
  int LineCount = 0;
  bool ignore = false;

  // Create and init ProgressDialog
  ProgressGlue::SetRange(1024);

  long file_size = reader.size();

  TempAirspaceType temp_area;
  asFileType filetype = ftUnknown;

  TCHAR *line;
  TCHAR *comment;
  // Iterate through the lines
  while ((line = reader.read()) != NULL) {
    // Increase line counter
    LineCount++;

    // Strip comments
    comment = _tcschr(line, _T('*'));
    if (comment != NULL)
      *comment = _T('\0');

    // Skip empty line
    if (string_is_empty(line))
      continue;

    if (filetype == ftUnknown) {
      filetype = DetectFileType(line);
      if (filetype == ftUnknown)
        continue;
    }

    // Parse the line
    if (filetype == ftOpenAir)
      if (!ParseLine(airspace_database, line, temp_area) &&
          !ShowParseWarning(LineCount, line))
        return false;

    if (filetype == ftTNP)
      if (!ParseLineTNP(airspace_database, line, temp_area, ignore) &&
          !ShowParseWarning(LineCount, line))
        return false;

    // Update the ProgressDialog
    ProgressGlue::SetValue(reader.tell() * 1024 / file_size);
  }

  if (filetype == ftUnknown) {
    MessageBoxX(_("Unknown Filetype."), _("Airspace"), MB_OK);
    return false;
  }

  // Process final area (if any)
  if (!temp_area.Waiting)
    temp_area.AddPolygon(airspace_database);

  return true;
}
