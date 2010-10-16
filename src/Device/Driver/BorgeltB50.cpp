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

#include "Device/Driver/BorgeltB50.hpp"
#include "Device/Parser.hpp"
#include "Device/Driver.hpp"
#include "Protection.hpp"
#include "Units.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/InputLine.hpp"

#include <stdlib.h>
#include <math.h>

class B50Device : public AbstractDevice {
public:
  virtual bool ParseNMEA(const char *line, struct NMEA_INFO *info,
                         bool enable_baro);
};

/*
Sentence has following format:

$PBB50,AAA,BBB.B,C.C,DDDDD,EE,F.FF,G,HH*CHK crlf

AAA = TAS 0 to 150 knots
BBB.B = Vario, -10 to +15 knots, negative sign for sink
C.C = MacCready 0 to 8.0 knots
DDDDD = IAS squared 0 to 22500
EE = bugs degradation, 0 = clean to 30 %
F.FF = Ballast 1.00 to 1.60
G = 0 in climb, 1 in cruise
HH = Outside airtemp in degrees celcius ( may have leading negative sign )
CHK = standard NMEA checksum
*/
static bool
PBB50(NMEAInputLine &line, NMEA_INFO *GPS_INFO)
{
  // $PBB50,100,0,10,1,10000,0,1,0,20*4A..
  // $PBB50,0,.0,.0,0,0,1.07,0,-228*58
  // $PBB50,14,-.2,.0,196,0,.92,0,-228*71

  fixed vtas, value;

  bool vtas_av = line.read_checked(vtas);

  GPS_INFO->TotalEnergyVarioAvailable = line.read_checked(value);
  if (GPS_INFO->TotalEnergyVarioAvailable) {
    GPS_INFO->TotalEnergyVario = Units::ToSysUnit(value, unKnots);

    TriggerVarioUpdate();
  }

  if (line.read_checked(value))
    GPS_INFO->MacCready = Units::ToSysUnit(value, unKnots);

  /// @todo: OLD_TASK device MC/bugs/ballast is currently not implemented, have to push MC to master
  ///  oldGlidePolar::SetMacCready(GPS_INFO->MacCready);

  GPS_INFO->AirspeedAvailable = line.read_checked(value) && vtas_av;
  if (GPS_INFO->AirspeedAvailable) {
    GPS_INFO->IndicatedAirspeed = Units::ToSysUnit(sqrt(value), unKnots);
    GPS_INFO->TrueAirspeed = Units::ToSysUnit(vtas, unKnots);
  }

  // RMN: Changed bugs-calculation, swapped ballast and bugs to suit
  // the B50-string for Borgelt, it's % degradation, for us, it is %
  // of max performance
  line.skip(2);
  /*

  JMW disabled bugs/ballast due to problems with test b50

  GPS_INFO->Bugs = 1.0 - max(0, min(30, line.read(0.0))) / 100.0;
  BUGS = GPS_INFO->Bugs;

  // for Borgelt it's % of empty weight,
  // for us, it's % of ballast capacity
  // RMN: Borgelt ballast->XCSoar ballast

  double bal = max(1.0, min(1.60, line.read(0.0))) - 1.0;
  if (WEIGHTS[2]>0) {
    GPS_INFO->Ballast = min(1.0, max(0.0,
                                     bal*(WEIGHTS[0]+WEIGHTS[1])/WEIGHTS[2]));
    BALLAST = GPS_INFO->Ballast;
  } else {
    GPS_INFO->Ballast = 0;
    BALLAST = 0;
  }
  // w0 pilot weight, w1 glider empty weight, w2 ballast weight
  */

  // inclimb/incruise 1=cruise,0=climb, OAT
  GPS_INFO->SwitchState.VarioCircling = line.read(false);
  if (GPS_INFO->SwitchState.VarioCircling) {
    triggerClimbEvent.trigger();
  } else {
    triggerClimbEvent.reset();
  }

  GPS_INFO->TemperatureAvailable = line.read_checked(value);
  if (GPS_INFO->TemperatureAvailable)
    GPS_INFO->OutsideAirTemperature = Units::ToSysUnit(value, unGradCelcius);

  return false;
}

bool
B50Device::ParseNMEA(const char *String, NMEA_INFO *GPS_INFO,
                     bool enable_baro)
{
  NMEAInputLine line(String);
  char type[16];
  line.read(type, 16);

  if (strcmp(type, "$PBB50") == 0)
    return PBB50(line, GPS_INFO);
  else
    return false;
}

static Device *
B50CreateOnPort(Port *com_port)
{
  return new B50Device();
}

const struct DeviceRegister b50Device = {
  _T("Borgelt B50"),
  drfGPS,
  B50CreateOnPort,
};
