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

#include "Device/Driver/Leonardo.hpp"
#include "Device/Parser.hpp"
#include "Device/Driver.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/InputLine.hpp"
#include "Protection.hpp"
#include "Units.hpp"

#include <stdlib.h>
#include <math.h>

class LeonardoDevice : public AbstractDevice {
private:
  Port *port;

public:
  LeonardoDevice(Port *_port):port(_port) {}

public:
  virtual bool ParseNMEA(const char *line, struct NMEA_INFO *info,
                         bool enable_baro);
};

static bool
ReadSpeedVector(NMEAInputLine &line, SpeedVector &value_r)
{
  fixed norm, bearing;

  bool bearing_valid = line.read_checked(bearing);
  bool norm_valid = line.read_checked(norm);

  if (bearing_valid && norm_valid) {
    value_r.norm = Units::ToSysUnit(norm, unKiloMeterPerHour);
    value_r.bearing = Angle::degrees(bearing);
    return true;
  } else
    return false;
}

/**
 * Parse a "$C" sentence.
 *
 * Example: "$C,+2025,-7,+18,+25,+29,122,314,314,0,-356,+25,45,T*3D"
 */
static bool
LeonardoParseC(NMEAInputLine &line, NMEA_INFO &info, bool enable_baro)
{
  fixed value;

  // 0 = altitude [m]
  if (line.read_checked(value) && enable_baro) {
    info.BaroAltitude = value;
    info.BaroAltitudeAvailable = true;
  }

  // 1 = vario [dm/s]
  info.TotalEnergyVarioAvailable = line.read_checked(value);
  if (info.TotalEnergyVarioAvailable)
    info.TotalEnergyVario = value / 10;

  // 2 = airspeed [km/h]
  /* XXX is that TAS or IAS? */
  info.AirspeedAvailable = line.read_checked(value);
  if (info.AirspeedAvailable) {
    info.TrueAirspeed = Units::ToSysUnit(value, unKiloMeterPerHour);
    info.IndicatedAirspeed = info.TrueAirspeed *
        AtmosphericPressure::AirDensityRatio(info.BaroAltitudeAvailable ?
            info.BaroAltitude : info.GPSAltitude);
  }

  // 3 = netto vario [dm/s]
  if (line.read_checked(value)) {
    info.NettoVario = value / 10;
    info.NettoVarioAvailable = true;
  } else
    /* short "$C" sentence ends after airspeed */
    return true;

  // 4 = temperature [deg C]
  fixed oat;
  info.TemperatureAvailable = line.read_checked(oat);
  if (info.TemperatureAvailable)
    info.OutsideAirTemperature = Units::ToSysUnit(oat, unGradCelcius);

  // 10 = wind speed [km/h]
  // 11 = wind direction [degrees]
  info.ExternalWindAvailable = ReadSpeedVector(line, info.wind);

  TriggerVarioUpdate();

  return true;
}

/**
 * Parse a "$D" sentence.
 *
 * Example: "$D,+0,100554,+25,18,+31,,0,-356,+25,+11,115,96*6A"
 */
static bool
LeonardoParseD(NMEAInputLine &line, NMEA_INFO &info)
{
  fixed value;

  // 0 = vario [dm/s]
  info.TotalEnergyVarioAvailable = line.read_checked(value);
  if (info.TotalEnergyVarioAvailable)
    info.TotalEnergyVario = value / 10;

  // 1 = air pressure [Pa]
  if (line.skip() == 0)
    /* short "$C" sentence ends after airspeed */
    return true;

  // 2 = netto vario [dm/s]
  if (line.read_checked(value)) {
    info.NettoVario = value / 10;
    info.NettoVarioAvailable = true;
  }

  // 3 = airspeed [km/h]
  /* XXX is that TAS or IAS? */
  info.AirspeedAvailable = line.read_checked(value);
  if (info.AirspeedAvailable) {
    info.TrueAirspeed = Units::ToSysUnit(value, unKiloMeterPerHour);
    info.IndicatedAirspeed = info.TrueAirspeed; // XXX convert properly
  }

  // 4 = temperature [deg C]
  fixed oat;
  info.TemperatureAvailable = line.read_checked(oat);
  if (info.TemperatureAvailable)
    info.OutsideAirTemperature = Units::ToSysUnit(oat, unGradCelcius);

  // 5 = compass [degrees]
  /* XXX unsupported by XCSoar */

  // 6 = optimal speed [km/h]
  /* XXX unsupported by XCSoar */

  // 7 = equivalent MacCready [cm/s]
  /* XXX unsupported by XCSoar */

  // 8 = wind speed [km/h]
  /* not used here, the "$C" record repeats it together with the
     direction */

  TriggerVarioUpdate();

  return true;
}

bool
LeonardoDevice::ParseNMEA(const char *_line, NMEA_INFO *info, bool enable_baro)
{
  NMEAInputLine line(_line);
  char type[16];
  line.read(type, 16);

  if (strcmp(type, "$C") == 0 || strcmp(type, "$c") == 0)
    return LeonardoParseC(line, *info, enable_baro);
  else if (strcmp(type, "$D") == 0 || strcmp(type, "$D") == 0)
    return LeonardoParseD(line, *info);
  else
    return false;
}

static Device *
LeonardoCreateOnPort(Port *com_port)
{
  return new LeonardoDevice(com_port);
}

const struct DeviceRegister leonardo_device_driver = {
  _T("Leonardo"),
  drfGPS | drfBaroAlt | drfSpeed | drfVario,
  LeonardoCreateOnPort,
};
