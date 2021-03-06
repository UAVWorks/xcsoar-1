/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2010 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

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

#include "Device/Driver/AltairPro.hpp"
#include "Device/Driver.hpp"
#include "Device/Internal.hpp"
#include "Device/Port.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/InputLine.hpp"
#include "Units.hpp"
#include "Waypoint/Waypoint.hpp"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <tchar.h>
#ifdef _UNICODE
#include <windows.h>
#endif

#define dim(x)            (sizeof(x)/sizeof(x[0]))
#define DECELWPNAMESIZE   24                        // max size of taskpoint name
#define DECELWPSIZE       DECELWPNAMESIZE + 25      // max size of WP declaration

class AltairProDevice : public AbstractDevice {
private:
  fixed lastAlt;
  bool last_enable_baro;
  Port *port;

  bool DeclareInternal(const struct Declaration *decl);
  void PutTurnPoint(const TCHAR *name, const Waypoint *waypoint);
  bool PropertySetGet(Port *port, char *Buffer, size_t size);
#ifdef _UNICODE
  bool PropertySetGet(Port *port, TCHAR *Buffer, size_t size);
#endif

public:
  AltairProDevice(Port *_port):lastAlt(fixed_zero), last_enable_baro(false), port(_port){}

public:
  virtual bool ParseNMEA(const char *line, struct NMEA_INFO *info,
                         bool enable_baro);
  virtual bool PutQNH(const AtmosphericPressure& pres);
  virtual bool Declare(const struct Declaration *declaration);
  virtual void OnSysTicker();
};

static bool
ReadAltitude(NMEAInputLine &line, fixed &value_r)
{
  fixed value;
  bool available = line.read_checked(value);
  char unit = line.read_first_char();
  if (!available)
    return false;

  if (unit == _T('f') || unit == _T('F'))
    value = Units::ToSysUnit(value, unFeet);

  value_r = value;
  return true;
}

bool
AltairProDevice::ParseNMEA(const char *String, NMEA_INFO *GPS_INFO,
                           bool enable_baro)
{
  NMEAInputLine line(String);
  char type[16];
  line.read(type, 16);

  // no propriatary sentence

  if (strcmp(type, "$PGRMZ") == 0) {
    bool available = ReadAltitude(line, lastAlt);
    if (enable_baro && available) {
      GPS_INFO->BaroAltitudeAvailable = true;
      GPS_INFO->BaroAltitude = GPS_INFO->pressure.AltitudeToQNHAltitude(lastAlt);
    }

    last_enable_baro = enable_baro;

    return true;
  }

  return false;

}


bool
AltairProDevice::Declare(const struct Declaration *decl)
{
  port->StopRxThread();
  port->SetRxTimeout(500); // set RX timeout to 500[ms]

  bool result = DeclareInternal(decl);

  port->SetRxTimeout(0); // clear timeout
  port->StartRxThread(); // restart RX thread}

  return result;
}


bool
AltairProDevice::DeclareInternal(const struct Declaration *decl)
{
  TCHAR Buffer[256];

  _stprintf(Buffer, _T("PDVSC,S,Pilot,%s"), decl-> PilotName);
  if (!PropertySetGet(port, Buffer, dim(Buffer)))
    return false;

  _stprintf(Buffer, _T("PDVSC,S,GliderID,%s"), decl->AircraftRego);
  if (!PropertySetGet(port, Buffer, dim(Buffer)))
    return false;

  _stprintf(Buffer, _T("PDVSC,S,GliderType,%s"), decl->AircraftType);
  if (!PropertySetGet(port, Buffer, dim(Buffer)))
    return false;

  /* TODO currently not supported by XCSOAR
   * Pilot2
   * CompetitionID
   * CompetitionClass
   * ObserverID
   * DeclDescription
   * DeclFlightDate
   */

  if (decl->size() > 1){

    PutTurnPoint(_T("DeclTakeoff"), NULL);
    PutTurnPoint(_T("DeclLanding"), NULL);

    PutTurnPoint(_T("DeclStart"), &decl->waypoints[0]);
    PutTurnPoint(_T("DeclFinish"), &decl->waypoints[decl->size()-1]);

    for (unsigned int index=1; index <= 10; index++){
      TCHAR TurnPointPropertyName[32];
      _stprintf(TurnPointPropertyName, _T("DeclTurnPoint%d"), index);

      if (index < decl->size()-1){
        PutTurnPoint(TurnPointPropertyName, &decl->waypoints[index]);
      } else {
        PutTurnPoint(TurnPointPropertyName, NULL);
      }

    }

  }

  _stprintf(Buffer, _T("PDVSC,S,DeclAction,DECLARE"));
  if (!PropertySetGet(port, Buffer, dim(Buffer)))
    return false;

  if (_tcscmp(&Buffer[9], _T("LOCKED")) == 0){
    // FAILED! try to declare a task on a airborn recorder
    return false;
  }

  // Buffer holds the declaration ticket.
  // but no one is intresting in that
  // eg "2010-11-21 13:01:43 (1)"

  return true;
}



bool
AltairProDevice::PropertySetGet(Port *port, char *Buffer, size_t size)
{
  assert(port != NULL);
  assert(Buffer != NULL);

  // eg $PDVSC,S,FOO,BAR*<cr>\r\n
  PortWriteNMEA(port, Buffer);

  Buffer[6] = _T('A');
  char *comma = strchr(&Buffer[8], ',');

  if (comma != NULL){
    comma[1] = '\0';

    // expect eg $PDVSC,A,FOO,
    if (port->ExpectString(Buffer)){

      // read value eg bar
      while(--size){
        char ch;

        if ((ch = port->GetChar()) == EOF)
          break;

        if (ch == '*'){
          Buffer = '\0';
          return true;
        }

        *Buffer++ = ch;

      }

    }
  }

  *Buffer = '\0';
  return false;
}

#ifdef _UNICODE
bool
AltairProDevice::PropertySetGet(Port *port, TCHAR *s, size_t size)
{
  assert(port != NULL);
  assert(s != NULL);

  char buffer[_tcslen(s) * 4 + 1];
  if (::WideCharToMultiByte(CP_ACP, 0, s, -1, buffer, sizeof(buffer),
                               NULL, NULL) <= 0)
    return false;

  if (!PropertySetGet(port, buffer, _tcslen(s) * 4 + 1))
    return false;

  if (::MultiByteToWideChar(CP_ACP, 0, buffer, -1, s, size) <= 0)
    return false;

  return true;

}
#endif

void
AltairProDevice::PutTurnPoint(const TCHAR *propertyName, const Waypoint *waypoint)
{

  TCHAR Name[DECELWPNAMESIZE];
  TCHAR Buffer[DECELWPSIZE*2];

  int DegLat, DegLon;
  double tmp, MinLat, MinLon;
  char NoS, EoW;

  if (waypoint != NULL){

    _tcsncpy(Name, waypoint->Name.c_str(), dim(Name)-1);
    Name[dim(Name)-1] = '\0';

    tmp = waypoint->Location.Latitude.value_degrees();

    if(tmp < 0){
      NoS = 'S';
      tmp *= -1;
    } else NoS = 'N';

    DegLat = (int)tmp;
    MinLat = tmp - DegLat;
    MinLat *= 60;
    MinLat *= 1000;

    tmp = waypoint->Location.Longitude.value_degrees();

    if (tmp < 0){
      EoW = 'W';
      tmp *= -1;
    } else EoW = 'E';

    DegLon = (int)tmp;
    MinLon = tmp  - DegLon;
    MinLon *= 60;
    MinLon *= 1000;

  } else {

    Name[0] = '\0';
    DegLat = 0;
    MinLat = 0;
    DegLon = 0;
    MinLon = 0;
    NoS = 'N';
    EoW = 'E';
  }

  _stprintf(Buffer, _T("PDVSC,S,%s,%02d%05.0f%c%03d%05.0f%c%s"),
            propertyName,
            DegLat, MinLat, NoS, DegLon, MinLon, EoW, Name
  );

  PropertySetGet(port, Buffer, dim(Buffer));

}


#include "DeviceBlackboard.hpp"

bool
AltairProDevice::PutQNH(const AtmosphericPressure &pres)
{
  // TODO code: JMW check sending QNH to Altair
  if (last_enable_baro)
    device_blackboard.SetBaroAlt(pres.AltitudeToQNHAltitude(fixed(lastAlt)));

  return true;
}

void
AltairProDevice::OnSysTicker()
{
  // Do To get IO data like temp, humid, etc
}

static Device *
AltairProCreateOnPort(Port *com_port)
{
  return new AltairProDevice(com_port);
}

const struct DeviceRegister atrDevice = {
  _T("Altair RU"),
  drfGPS | drfBaroAlt | drfLogger,
  AltairProCreateOnPort,
};
