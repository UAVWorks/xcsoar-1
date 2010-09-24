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

#include "Device/Parser.hpp"
#include "Device/device.hpp"
#include "Protection.hpp"
#include "Device/Geoid.h"
#include "FLARM/FlarmCalculations.h"
#include "Math/Earth.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/Checksum.h"
#include "NMEA/InputLine.hpp"
#include "StringUtil.hpp"
#include "InputEvents.h"
#include "Compatibility/string.h" /* for _ttoi() */

#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include <algorithm>

using std::min;
using std::max;

static FlarmCalculations flarmCalculations;

bool NMEAParser::ignore_checksum;

int NMEAParser::StartDay = -1;

/**
 * Constructor of the NMEAParser class
 * @return NMEAParser object
 */
NMEAParser::NMEAParser() {
  Reset();
}

/**
 * Resets the NMEAParser
 */
void
NMEAParser::Reset(void)
{
  gpsValid = false;
  isFlarm = false;
  activeGPS = true;
  GGAAvailable = false;
  RMZAvailable = false;
  RMAAvailable = false;
  LastTime = fixed_zero;
}

/**
 * Parses a provided NMEA String into a GPS_INFO struct
 * @param String NMEA string
 * @param GPS_INFO GPS_INFO output struct
 * @return Parsing success
 */
bool
NMEAParser::ParseNMEAString_Internal(const char *String, NMEA_INFO *GPS_INFO)
{
  if (!NMEAChecksum(String))
    return false;

  NMEAInputLine line(String);

  char type[16];
  line.read(type, 16);

  if (type[0] != '$')
    return false;

  // if (proprietary sentence) ...
  if (type[1] == 'P') {
    // Airspeed and vario sentence
    if (strcmp(type + 1, "PTAS1") == 0)
      return PTAS1(line, GPS_INFO);

    // FLARM sentences
    if (strcmp(type + 1, "PFLAA") == 0)
      return PFLAA(line, GPS_INFO);

    if (strcmp(type + 1, "PFLAU") == 0)
      return PFLAU(line, GPS_INFO->flarm);

    // Garmin altitude sentence
    if (strcmp(type + 1, "PGRMZ") == 0)
      return RMZ(line, GPS_INFO);

    return false;
  }

  if (strcmp(type + 3, "GSA") == 0)
    return GSA(line, GPS_INFO);

  if (strcmp(type + 3, "GLL") == 0)
    //    return GLL(line, GPS_INFO);
    return false;

  if (strcmp(type + 3, "RMB") == 0)
    //return RMB(line, GPS_INFO);
    return false;

  if (strcmp(type + 3, "RMC") == 0)
    return RMC(line, GPS_INFO);

  if (strcmp(type + 3, "GGA") == 0)
    return GGA(line, GPS_INFO);

  return false;
}

/**
 * Converts a given double and 'E' and 'W' to the appropriate signed double
 * @param in Input value
 * @param EoW Input direction
 * @return Signed value
 */
static double
EastOrWest(double in, char EoW)
{
  if (EoW == 'W')
    return -in;
  else
    return in;
}

/**
 * Converts a given double and 'N' and 'S' to the appropriate signed double
 * @param in Input value
 * @param NoS Input direction
 * @return Signed value
 */
static double
NorthOrSouth(double in, char NoS)
{
  if (NoS == 'S')
    return -in;
  else
    return in;
}

/*
static double
LeftOrRight(double in, char LoR)
{
  if(LoR == 'L')
    return -in;
  else
    return in;
}
*/

/**
 * Parses whether the given character (GPS status) should create a navigational warning
 * @param c GPS status
 * @return True if GPS fix not found or invalid
 */
static bool
NAVWarn(char c)
{
  return c != 'A';
}

/**
 * Converts the mixed lat/lon format to pure degrees
 *
 * Example:
 * 3845.587
 * => 38 + 45.587/60
 * = 38.75978333 degrees
 * @param mixed Mixed formated string
 * @return Degrees
 */
static double
MixedFormatToDegrees(double mixed)
{
  double mins, degrees;

  degrees = (int)(mixed / 100);
  mins = (mixed - degrees * 100) / 60;

  return degrees + mins;
}

static bool
ReadDoubleAndChar(NMEAInputLine &line, double &d, char &ch)
{
  bool success = line.read_checked(d);
  ch = line.read_first_char();
  return success;
}

static bool
ReadFixedAndChar(NMEAInputLine &line, fixed &d, char &ch)
{
  bool success = line.read_checked(d);
  ch = line.read_first_char();
  return success;
}

#include <stdio.h>
static bool
ReadLatitude(NMEAInputLine &line, Angle &value_r)
{
  double value;
  char ch;

  if (!ReadDoubleAndChar(line, value, ch))
    return false;

  value = NorthOrSouth(MixedFormatToDegrees(value), ch);
  value_r = Angle::degrees(fixed(value));
  return true;
}

static bool
ReadLongitude(NMEAInputLine &line, Angle &value_r)
{
  double value;
  char ch;

  if (!ReadDoubleAndChar(line, value, ch))
    return false;

  value = EastOrWest(MixedFormatToDegrees(value), ch);
  value_r = Angle::degrees(fixed(value));
  return true;
}

static bool
ReadGeoPoint(NMEAInputLine &line, GEOPOINT &value_r)
{
  GEOPOINT value;

  bool latitude_valid = ReadLatitude(line, value.Latitude);
  bool longitude_valid = ReadLongitude(line, value.Longitude);
  if (latitude_valid && longitude_valid) {
    value_r = value;
    return true;
  } else
    return false;
}

bool
NMEAParser::ReadAltitude(NMEAInputLine &line, fixed &value_r)
{
  fixed value;
  char format;
  if (!ReadFixedAndChar(line, value, format))
    return false;

  if (format == 'f' || format == 'F')
    value = Units::ToSysUnit(value, unFeet);

  value_r = value;
  return true;
}

/**
 * Calculates a seconds-based FixTime and corrects it
 * in case over passing the UTC midnight mark
 * @param FixTime NMEA format fix time (HHMMSS)
 * @param GPS_INFO GPS_INFO struct to parse into
 * @return Seconds-based FixTime
 */
fixed
NMEAParser::TimeModify(fixed FixTime, BrokenDateTime &date_time)
{
  fixed hours, mins, secs;

  // Calculate Hour
  hours = FixTime / 10000;
  date_time.hour = (int)hours;

  // Calculate Minute
  mins = FixTime / 100;
  mins = mins - fixed(date_time.hour) * 100;
  date_time.minute = (int)mins;

  // Calculate Second
  secs = FixTime - fixed(date_time.hour * 10000 + date_time.minute * 100);
  date_time.second = (int)secs;

  // FixTime is now seconds-based instead of mixed format
  FixTime = secs + fixed(date_time.minute * 60 + date_time.hour * 3600);

  // If (StartDay not yet set and available) set StartDate;
  if ((StartDay == -1) && (date_time.day != 0))
    StartDay = date_time.day;

  if (StartDay != -1) {
    if (date_time.day < StartDay)
      // detect change of month (e.g. day=1, startday=31)
      StartDay = date_time.day - 1;

    int day_difference = date_time.day - StartDay;
    if (day_difference > 0)
      // Add seconds to fix time so time doesn't wrap around when
      // going past midnight in UTC
      FixTime += fixed(day_difference * 86400);
  }

  return FixTime;
}

/**
 * Checks whether time has advanced since last call and
 * updates the GPS_info if necessary
 * @param ThisTime Current time
 * @param GPS_INFO GPS_INFO struct to update
 * @return True if time has advanced since last call
 */
bool
NMEAParser::TimeHasAdvanced(fixed ThisTime, NMEA_INFO *GPS_INFO)
{
  if (ThisTime < LastTime) {
    LastTime = ThisTime;
    StartDay = -1; // reset search for the first day
    return false;
  } else {
    GPS_INFO->Time = ThisTime;
    LastTime = ThisTime;
    return true;
  }
}

/**
 * Parses a GSA sentence
 *
 * $--GSA,a,a,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x.x,x.x,x.x*hh
 *
 * Field Number:
 *  1) Selection mode
 *	       M=Manual, forced to operate in 2D or 3D
 *	       A=Automatic, 3D/2D
 *  2) Mode (1 = no fix, 2 = 2D fix, 3 = 3D fix)
 *  3) ID of 1st satellite used for fix
 *  4) ID of 2nd satellite used for fix
 *  ...
 *  14) ID of 12th satellite used for fix
 *  15) PDOP
 *  16) HDOP
 *  17) VDOP
 *  18) checksum
 * @param String Input string
 * @param params Parameter array
 * @param nparams Number of parameters
 * @param GPS_INFO GPS_INFO struct to parse into
 * @return Parsing success
 */
bool
NMEAParser::GSA(NMEAInputLine &line, NMEA_INFO *GPS_INFO)
{
  if (GPS_INFO->gps.Replay)
    return true;

  line.skip(2);

  // satellites are in items 4-15 of GSA string (4-15 is 1-indexed)
  for (unsigned i = 0; i < MAXSATELLITES; i++)
    GPS_INFO->gps.SatelliteIDs[i] = line.read(0);

  GSAAvailable = true;
  return true;
}

/**
 * Parses a GLL sentence
 *
 * $--GLL,llll.ll,a,yyyyy.yy,a,hhmmss.ss,a,m,*hh
 *
 * Field Number:
 *  1) Latitude
 *  2) N or S (North or South)
 *  3) Longitude
 *  4) E or W (East or West)
 *  5) Universal Time Coordinated (UTC)
 *  6) Status A - Data Valid, V - Data Invalid
 *  7) FAA mode indicator (NMEA 2.3 and later)
 *  8) Checksum
 * @param String Input string
 * @param params Parameter array
 * @param nparams Number of parameters
 * @param GPS_INFO GPS_INFO struct to parse into
 * @return Parsing success
 */
bool
NMEAParser::GLL(NMEAInputLine &line, NMEA_INFO *GPS_INFO)
{
  GEOPOINT location;
  bool valid_location = ReadGeoPoint(line, location);

  fixed ThisTime = TimeModify(line.read(fixed_zero), GPS_INFO->DateTime);

  gpsValid = !NAVWarn(line.read_first_char());

  if (!activeGPS)
    return true;

  if (GPS_INFO->gps.Replay)
    // block actual GPS signal
    return true;

  GPS_INFO->gps.NAVWarning = !gpsValid;

  if (!TimeHasAdvanced(ThisTime, GPS_INFO))
    return false;

  if (valid_location)
    GPS_INFO->Location = location;
  else
    GPS_INFO->gps.NAVWarning = true;

  return true;
}

/**
 * Parses a RMB sentence
 * (not used anymore)
 *
 * $--RMB,A,x.x,a,c--c,c--c,llll.ll,a,yyyyy.yy,a,x.x,x.x,x.x,A,m,*hh
 *
 * Field Number:
 *  1) Status, A= Active, V = Void
 *  2) Cross Track error - nautical miles
 *  3) Direction to Steer, Left or Right
 *  4) TO Waypoint ID
 *  5) FROM Waypoint ID
 *  6) Destination Waypoint Latitude
 *  7) N or S
 *  8) Destination Waypoint Longitude
 *  9) E or W
 * 10) Range to destination in nautical miles
 * 11) Bearing to destination in degrees True
 * 12) Destination closing velocity in knots
 * 13) Arrival Status, A = Arrival Circle Entered
 * 14) FAA mode indicator (NMEA 2.3 and later)
 * 15) Checksum
 * @param String Input string
 * @param params Parameter array
 * @param nparams Number of parameters
 * @param GPS_INFO GPS_INFO struct to parse into
 * @return Parsing success
 */
bool
NMEAParser::RMB(NMEAInputLine &line, NMEA_INFO *GPS_INFO)
{
  (void)line;
  (void)GPS_INFO;

  /* we calculate all this stuff now
  char ctemp[MAX_NMEA_LEN];

  GPS_INFO->NAVWarning = NAVWarn(params[0][0]);

  GPS_INFO->CrossTrackError = NAUTICALMILESTOMETRES * strtod(params[1], NULL);
  GPS_INFO->CrossTrackError = LeftOrRight(GPS_INFO->CrossTrackError,params[2][0]);

  strcpy(ctemp, params[4]);
  ctemp[WAY_POINT_ID_SIZE] = '\0';
  strcpy(GPS_INFO->WaypointID,ctemp);

  GPS_INFO->WaypointDistance = NAUTICALMILESTOMETRES * strtod(params[9], NULL);
  GPS_INFO->WaypointBearing = strtod(params[10], NULL);
  GPS_INFO->WaypointSpeed =  Units::ToSysUnit(strtod(params[11], NULL), unKnots);
  */

  return true;
}

/**
 * Parses a RMC sentence
 *
 * $--RMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,xxxx,x.x,a,m,*hh
 *
 * Field Number:
 *  1) UTC Time
 *  2) Status, V=Navigation receiver warning A=Valid
 *  3) Latitude
 *  4) N or S
 *  5) Longitude
 *  6) E or W
 *  7) Speed over ground, knots
 *  8) Track made good, degrees true
 *  9) Date, ddmmyy
 * 10) Magnetic Variation, degrees
 * 11) E or W
 * 12) FAA mode indicator (NMEA 2.3 and later)
 * 13) Checksum
 * @param String Input string
 * @param params Parameter array
 * @param nparams Number of parameters
 * @param GPS_INFO GPS_INFO struct to parse into
 * @return Parsing success
 */
bool
NMEAParser::RMC(NMEAInputLine &line, NMEA_INFO *GPS_INFO)
{
  fixed ThisTime = TimeModify(line.read(fixed_zero), GPS_INFO->DateTime);

  gpsValid = !NAVWarn(line.read_first_char());

  GEOPOINT location;
  bool valid_location = ReadGeoPoint(line, location);

  GPS_STATE &gps = GPS_INFO->gps;

  gps.Connected = 2;

  if (!activeGPS)
    return true;

  fixed speed = line.read(fixed_zero);
  gps.MovementDetected = speed > fixed_two;

  if (gps.Replay)
    // block actual GPS signal if not moving and a log is being replayed
    return true;

  gps.NAVWarning = !gpsValid;

  fixed TrackBearing = line.read(fixed_zero);

  // JMW get date info first so TimeModify is accurate
  char date_buffer[9];
  line.read(date_buffer, 9);

  GPS_INFO->DateTime.year = atoi(&date_buffer[4]) + 2000;
  date_buffer[4] = '\0';
  GPS_INFO->DateTime.month = atoi(&date_buffer[2]);
  date_buffer[2] = '\0';
  GPS_INFO->DateTime.day = atoi(&date_buffer[0]);

  if (!TimeHasAdvanced(ThisTime, GPS_INFO))
    return false;

  if (valid_location)
    GPS_INFO->Location = location;
  else
    GPS_INFO->gps.NAVWarning = true;

  GPS_INFO->GroundSpeed = Units::ToSysUnit(speed, unKnots);

  if (GPS_INFO->GroundSpeed > fixed_one) {
    // JMW don't update bearing unless we're moving
    GPS_INFO->TrackBearing = Angle::degrees(TrackBearing).as_bearing();
  }

  if (!gps.Replay) {
    if (RMZAvailable) {
      // JMW changed from Altitude to BaroAltitude
      GPS_INFO->BaroAltitudeAvailable = true;
      GPS_INFO->BaroAltitude = RMZAltitude;
    } else if (RMAAvailable) {
      // JMW changed from Altitude to BaroAltitude
      GPS_INFO->BaroAltitudeAvailable = true;
      GPS_INFO->BaroAltitude = RMAAltitude;
    }
  }

  if (!GGAAvailable) {
    // update SatInUse, some GPS receiver don't emit GGA sentence
    if (!gpsValid)
      gps.SatellitesUsed = 0;
    else
      gps.SatellitesUsed = -1;
  }

  // say we are updated every time we get this,
  // so infoboxes get refreshed if GPS connected
  TriggerGPSUpdate();

  return true;
}

/**
 * Parses a GGA sentence
 *
 * $--GGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh
 *
 * Field Number:
 *  1) Universal Time Coordinated (UTC)
 *  2) Latitude
 *  3) N or S (North or South)
 *  4) Longitude
 *  5) E or W (East or West)
 *  6) GPS Quality Indicator,
 *     0 - fix not available,
 *     1 - GPS fix,
 *     2 - Differential GPS fix
 *     (values above 2 are 2.3 features)
 *     3 = PPS fix
 *     4 = Real Time Kinematic
 *     5 = Float RTK
 *     6 = estimated (dead reckoning)
 *     7 = Manual input mode
 *     8 = Simulation mode
 *  7) Number of satellites in view, 00 - 12
 *  8) Horizontal Dilution of precision (meters)
 *  9) Antenna Altitude above/below mean-sea-level (geoid) (in meters)
 * 10) Units of antenna altitude, meters
 * 11) Geoidal separation, the difference between the WGS-84 earth
 *     ellipsoid and mean-sea-level (geoid), "-" means mean-sea-level
 *     below ellipsoid
 * 12) Units of geoidal separation, meters
 * 13) Age of differential GPS data, time in seconds since last SC104
 *     type 1 or 9 update, null field when DGPS is not used
 * 14) Differential reference station ID, 0000-1023
 * 15) Checksum
 * @param String Input string
 * @param params Parameter array
 * @param nparams Number of parameters
 * @param GPS_INFO GPS_INFO struct to parse into
 * @return Parsing success
 */
bool
NMEAParser::GGA(NMEAInputLine &line, NMEA_INFO *GPS_INFO)
{
  GPS_STATE &gps = GPS_INFO->gps;

  if (gps.Replay)
    return true;

  GGAAvailable = true;

  fixed ThisTime = TimeModify(line.read(fixed_zero), GPS_INFO->DateTime);

  GEOPOINT location;
  bool valid_location = ReadGeoPoint(line, location);

  gps.FixQuality = line.read(0);
  if (gps.FixQuality != 1 && gps.FixQuality != 2)
    gpsValid = false;

  int nSatellites = min(16, line.read(0));
  if (nSatellites == 0)
    gpsValid = false;

  if (!activeGPS)
    return true;

  gps.SatellitesUsed = nSatellites;

  if (!TimeHasAdvanced(ThisTime, GPS_INFO))
    return false;

  if (valid_location)
    GPS_INFO->Location = location;
  else
    GPS_INFO->gps.NAVWarning = true;

  gps.HDOP = line.read(0.0);

  if (RMZAvailable) {
    GPS_INFO->BaroAltitudeAvailable = true;
    GPS_INFO->BaroAltitude = RMZAltitude;
  } else if (RMAAvailable) {
    GPS_INFO->BaroAltitudeAvailable = true;
    GPS_INFO->BaroAltitude = RMAAltitude;
  }

  // VENTA3 CONDOR ALTITUDE
  // "Altitude" should always be GPS Altitude.

  bool altitude_available = ReadAltitude(line, GPS_INFO->GPSAltitude);
  if (!altitude_available)
    GPS_INFO->GPSAltitude = fixed_zero;

  fixed GeoidSeparation;
  if (ReadAltitude(line, GeoidSeparation)) {
    // No real need to parse this value,
    // but we do assume that no correction is required in this case

    if (!altitude_available)
      /* Some devices, such as the "LG Incite Cellphone" seem to be
         severely bugged, and report the GPS altitude in the Geoid
         column.  That sucks! */
      GPS_INFO->GPSAltitude = GeoidSeparation;
  } else {
    // need to estimate Geoid Separation internally (optional)
    // FLARM uses MSL altitude
    //
    // Some others don't.
    //
    // If the separation doesn't appear in the sentence,
    // we can assume the GPS unit is giving ellipsoid height
    //
    if (!HaveCondorDevice()) {
      // JMW TODO really need to know the actual device..
      GeoidSeparation = LookupGeoidSeparation(GPS_INFO->Location);
      GPS_INFO->GPSAltitude -= GeoidSeparation;
    }
  }

  return true;
}

/**
 * Parses a RMZ sentence
 * @param String Input string
 * @param params Parameter array
 * @param nparams Number of parameters
 * @param GPS_INFO GPS_INFO struct to parse into
 * @return Parsing success
 */
bool
NMEAParser::RMZ(NMEAInputLine &line, NMEA_INFO *GPS_INFO)
{
  //JMW?  RMZAltitude = GPS_INFO->pressure.AltitudeToQNHAltitude(RMZAltitude);
  RMZAvailable = ReadAltitude(line, RMZAltitude);

  if (RMZAvailable && !devHasBaroSource() && !GPS_INFO->gps.Replay) {
    // JMW no in-built baro sources, so use this generic one
    GPS_INFO->BaroAltitudeAvailable = true;
    GPS_INFO->BaroAltitude = RMZAltitude;
  }

  return false;
}

/**
 * Parses a RMA sentence
 * (not in use and maybe faulty(?))
 *
 * $--RMA,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,x.x,x.x,x.x,a*hh
 *
 * Field Number:
 *  1) Blink Warning
 *  2) Latitude
 *  3) N or S
 *  4) Longitude
 *  5) E or W
 *  6) Time Difference A, uS
 *  7) Time Difference B, uS
 *  8) Speed Over Ground, Knots
 *  9) Track Made Good, degrees true
 * 10) Magnetic Variation, degrees
 * 11) E or W
 * 12) Checksum
 * @param String Input string
 * @param params Parameter array
 * @param nparams Number of parameters
 * @param GPS_INFO GPS_INFO struct to parse into
 * @return Parsing success
 */
bool
NMEAParser::RMA(NMEAInputLine &line, NMEA_INFO *GPS_INFO)
{
  //JMW?  RMAAltitude = GPS_INFO->pressure.AltitudeToQNHAltitude(RMAAltitude);
  RMAAvailable = ReadAltitude(line, RMAAltitude);

  if (RMAAvailable && !devHasBaroSource() && !GPS_INFO->gps.Replay) {
    // JMW no in-built baro sources, so use this generic one
    GPS_INFO->BaroAltitudeAvailable = true;
    GPS_INFO->BaroAltitude = RMAAltitude;
  }

  return false;
}

/**
 * Calculates the checksum of the provided NMEA string and
 * compares it to the provided checksum
 * @param String NMEA string
 * @return True if checksum correct
 */
bool
NMEAParser::NMEAChecksum(const char *String)
{
  if (ignore_checksum)
    return true;

  unsigned char ReadCheckSum, CalcCheckSum;
  char c1, c2;
  unsigned char v1 = 0, v2 = 0;
  const char *pEnd;

  pEnd = strchr(String, '*');
  if(pEnd == NULL)
    return false;

  if (strlen(pEnd) < 3)
    return false;

  c1 = pEnd[1], c2 = pEnd[2];

  // what's this for?
  // iswdigit('0');

  if (_istdigit(c1))
    v1 = (unsigned char)(c1 - '0');
  if (_istdigit(c2))
    v2 = (unsigned char)(c2 - '0');
  if (_istalpha(c1))
    v1 = (unsigned char)(c1 - 'A' + 10);
  if (_istalpha(c2))
    v2 = (unsigned char)(c2 - 'A' + 10);

  ReadCheckSum = (unsigned char)((v1 << 4) + v2);
  CalcCheckSum = ::NMEAChecksum(String + 1, pEnd - String - 1);

  if (CalcCheckSum == ReadCheckSum)
    return true;
  else
    return false;
}

/**
 * Parses a PTAS1 sentence
 * @param String Input string
 * @param params Parameter array
 * @param nparams Number of parameters
 * @param GPS_INFO GPS_INFO struct to parse into
 * @return Parsing success
 */
bool
NMEAParser::PTAS1(NMEAInputLine &line, NMEA_INFO *GPS_INFO)
{
  fixed wnet;
  if (line.read_checked(wnet)) {
    GPS_INFO->TotalEnergyVario = Units::ToSysUnit((wnet - fixed(200)) / 10,
                                                  unKnots);
    GPS_INFO->TotalEnergyVarioAvailable = true;
  }

  line.skip(); // average vario +200

  fixed baralt;
  bool valid_baralt = false;
  if (line.read_checked(baralt)) {
    baralt = max(fixed_zero, Units::ToSysUnit(baralt - fixed(2000), unFeet));
    valid_baralt = true;

    GPS_INFO->BaroAltitudeAvailable = true;
    GPS_INFO->BaroAltitude = GPS_INFO->pressure.AltitudeToQNHAltitude(baralt);
  }

  fixed vtas;
  bool valid_vtas = false;
  if (line.read_checked(vtas)) {
    vtas = Units::ToSysUnit(vtas, unKnots);

    GPS_INFO->AirspeedAvailable = true;
    GPS_INFO->TrueAirspeed = vtas;
  }

  if (valid_baralt && valid_vtas)
    GPS_INFO->IndicatedAirspeed =
      vtas / GPS_INFO->pressure.AirDensityRatio(baralt);

  TriggerVarioUpdate();

  return false;
}

/**
 * Parses a PFLAU sentence
 * (Operating status and priority intruder and obstacle data)
 * @param String Input string
 * @param params Parameter array
 * @param nparams Number of parameters
 * @param GPS_INFO GPS_INFO struct to parse into
 * @return Parsing success
 * @see http://flarm.com/support/manual/FLARM_DataportManual_v4.06E.pdf
 */
bool
NMEAParser::PFLAU(NMEAInputLine &line, FLARM_STATE &flarm)
{
  static int old_flarm_rx = 0;

  flarm.FLARM_Available = true;
  isFlarm = true;

  // PFLAU,<RX>,<TX>,<GPS>,<Power>,<AlarmLevel>,<RelativeBearing>,<AlarmType>,
  //   <RelativeVertical>,<RelativeDistance>(,<ID>)
  flarm.FLARM_RX = line.read(0);
  flarm.FLARM_TX = line.read(0);
  flarm.FLARM_GPS = line.read(0);
  line.skip();
  flarm.FLARM_AlarmLevel = line.read(0);

  // process flarm updates

  if (flarm.FLARM_RX && old_flarm_rx == 0)
    // traffic has appeared..
    InputEvents::processGlideComputer(GCE_FLARM_TRAFFIC);

  if (flarm.FLARM_RX == 0 && old_flarm_rx)
    // traffic has disappeared..
    InputEvents::processGlideComputer(GCE_FLARM_NOTRAFFIC);

  // TODO feature: add another event for new traffic.

  old_flarm_rx = flarm.FLARM_RX;

  return false;
}

/**
 * Parses a PFLAA sentence
 * (Data on other moving objects around)
 * @param String Input string
 * @param params Parameter array
 * @param nparams Number of parameters
 * @param GPS_INFO GPS_INFO struct to parse into
 * @return Parsing success
 * @see http://flarm.com/support/manual/FLARM_DataportManual_v4.06E.pdf
 */
bool
NMEAParser::PFLAA(NMEAInputLine &line, NMEA_INFO *GPS_INFO)
{
  FLARM_STATE &flarm = GPS_INFO->flarm;

  isFlarm = true;

  // calculate relative east and north projection to lat/lon

  Angle delta_lat = Angle::degrees(fixed(0.01));
  Angle delta_lon = Angle::degrees(fixed(0.01));

  GEOPOINT plat = GPS_INFO->Location;
  plat.Latitude += delta_lat;
  GEOPOINT plon = GPS_INFO->Location;
  plon.Longitude += delta_lon;

  fixed dlat = Distance(GPS_INFO->Location, plat);
  fixed dlon = Distance(GPS_INFO->Location, plon);

  fixed FLARM_NorthingToLatitude(0);
  fixed FLARM_EastingToLongitude(0);

  if (positive(fabs(dlat)) && positive(fabs(dlon))) {
    FLARM_NorthingToLatitude = delta_lat.value_degrees() / dlat;
    FLARM_EastingToLongitude = delta_lon.value_degrees() / dlon;
  }

  // PFLAA,<AlarmLevel>,<RelativeNorth>,<RelativeEast>,<RelativeVertical>,
  //   <IDType>,<ID>,<Track>,<TurnRate>,<GroundSpeed>,<ClimbRate>,<AcftType>
  FLARM_TRAFFIC traffic;
  traffic.AlarmLevel = line.read(0);
  traffic.RelativeNorth = line.read(fixed_zero);
  traffic.RelativeEast = line.read(fixed_zero);
  traffic.RelativeAltitude = line.read(fixed_zero);
  traffic.IDType = line.read(0);

  // 5 id, 6 digit hex
  char id_string[16];
  line.read(id_string, 16);
  traffic.ID.parse(id_string, NULL);

  traffic.TrackBearing = Angle::degrees(line.read(fixed_zero));
  traffic.TurnRate = line.read(fixed_zero);
  traffic.Speed = line.read(fixed_zero);
  traffic.ClimbRate = line.read(fixed_zero);
  traffic.Type = (FLARM_TRAFFIC::AircraftType)line.read(0);

  FLARM_TRAFFIC *flarm_slot = flarm.FindTraffic(traffic.ID);
  if (flarm_slot == NULL) {
    flarm_slot = flarm.AllocateTraffic();
    if (flarm_slot == NULL)
      // no more slots available
      return false;

    flarm_slot->ID = traffic.ID;

    flarm.NewTraffic = true;
    InputEvents::processGlideComputer(GCE_FLARM_NEWTRAFFIC);
  }

  // set time of fix to current time
  flarm_slot->Time_Fix = GPS_INFO->Time;

  // PFLAA,<AlarmLevel>,<RelativeNorth>,<RelativeEast>,<RelativeVertical>,
  //   <IDType>,<ID>,<Track>,<TurnRate>,<GroundSpeed>,<ClimbRate>,<AcftType>
  flarm_slot->AlarmLevel = traffic.AlarmLevel;
  flarm_slot->RelativeNorth = traffic.RelativeNorth;
  flarm_slot->RelativeEast = traffic.RelativeEast;
  flarm_slot->RelativeAltitude = traffic.RelativeAltitude;
  flarm_slot->IDType = traffic.IDType;
  flarm_slot->TrackBearing = traffic.TrackBearing;
  flarm_slot->TurnRate = traffic.TurnRate;
  flarm_slot->Speed = traffic.Speed;
  flarm_slot->ClimbRate = traffic.ClimbRate;
  flarm_slot->Type = traffic.Type;

  // 1 relativenorth, meters
  flarm_slot->Location.Latitude = Angle::degrees(flarm_slot->RelativeNorth
                                        * FLARM_NorthingToLatitude) + GPS_INFO->Location.Latitude;

  // 2 relativeeast, meters
  flarm_slot->Location.Longitude = Angle::degrees(flarm_slot->RelativeEast
                                         * FLARM_EastingToLongitude) + GPS_INFO->Location.Longitude;

  // alt
  flarm_slot->Altitude = flarm_slot->RelativeAltitude + GPS_INFO->GPSAltitude;

  flarm_slot->Average30s = flarmCalculations.Average30s(flarm_slot->ID,
                                                        GPS_INFO->Time,
                                                        flarm_slot->Altitude);

  // QUESTION TB: never returns true?!
  return false;
}

/**
 * This function creates some simulated traffic for FLARM debugging
 * @param GPS_INFO Pointer to the NMEA_INFO struct
 */
void NMEAParser::TestRoutine(NMEA_INFO *GPS_INFO) {
  static int i = 90;

  i++;
  if (i > 255)
    i = 0;

  if (i > 80)
    return;

  const Angle angle = Angle::degrees(fixed((i * 360) / 255)).as_bearing();

  // PFLAU,<RX>,<TX>,<GPS>,<Power>,<AlarmLevel>,<RelativeBearing>,<AlarmType>,
  //   <RelativeVertical>,<RelativeDistance>(,<ID>)
  int h1;
  int n1;
  int e1;
  int t1;
  unsigned l;
  h1 = (angle.ifastsine()) / 7;
  n1 = (angle.ifastsine()) / 2 - 200;
  e1 = (int)((angle.ifastcosine()) / 1.5);
  t1 = -angle.as_bearing().value_degrees();

  l = (i % 30 > 13 ? 0 : (i % 30 > 5 ? 2 : 1));
  int h2;
  int n2;
  int e2;
  int t2;
  Angle dangle = (angle + Angle::degrees(fixed(120))).as_bearing();
  Angle hangle = dangle; hangle.flip(); hangle = hangle.as_bearing();

  h2 = (angle.ifastcosine()) / 10;
  n2 = (int)((dangle.ifastsine()) / 1.20) + 300;
  e2 = (dangle.ifastcosine()) + 500;
  t2 = hangle.value_degrees();

  // PFLAA,<AlarmLevel>,<RelativeNorth>,<RelativeEast>,<RelativeVertical>,
  //   <IDType>,<ID>,<Track>,<TurnRate>,<GroundSpeed>,<ClimbRate>,<AcftType>
  char t_laa1[50];
  sprintf(t_laa1, "%d,%d,%d,%d,2,DDA85C,%d,0,0,0,1", l, n1, e1, h1, t1);
  char t_laa2[50];
  sprintf(t_laa2, "0,%d,%d,%d,2,AA9146,%d,0,0,0,1", n2, e2, h2, t2);

  char t_lau[50];
  sprintf(t_lau, "2,1,2,1,%d", l);

  GPS_INFO->flarm.FLARM_Available = true;

  NMEAInputLine line(t_lau);
  PFLAU(line, GPS_INFO->flarm);

  line = NMEAInputLine(t_laa1);
  PFLAA(line, GPS_INFO);

  line = NMEAInputLine(t_laa2);
  PFLAA(line, GPS_INFO);
}
