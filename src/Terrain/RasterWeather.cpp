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

#include "Terrain/RasterWeather.hpp"
#include "Terrain/RasterMap.hpp"
#include "Language.hpp"
#include "Units.hpp"
#include "LocalPath.hpp"
#include "LocalTime.hpp"
#include "OS/PathName.hpp"
#include "OS/FileUtil.hpp"
#include "ProgressGlue.hpp"
#include "zzip/zzip.h"
#include "Engine/Math/Earth.hpp"

#include <assert.h>
#include <tchar.h>
#include <stdio.h>
#include <windef.h> // for MAX_PATH

struct WeatherDescriptor {
  const TCHAR *name;
  const TCHAR *label;
  const TCHAR *help;
};

static const WeatherDescriptor WeatherDescriptors[RasterWeather::MAX_WEATHER_MAP] = {
  {
    NULL,
    N_("Terrain"),
    N_("Display terrain on map, no weather data displayed."),
  },
  {
    _T("wstar"),
    N_("W*"),
    N_("Average dry thermal updraft strength near mid-BL height.  Subtract glider descent rate to get average vario reading for cloudless thermals.  Updraft strengths will be stronger than this forecast if convective clouds are present, since cloud condensation adds buoyancy aloft (i.e. this neglects \"cloudsuck\").  W* depends upon both the surface heating and the BL depth."),
  },
  {
    _T("blwindspd"),
    N_("BL Wind spd"),
    N_("The speed and direction of the vector-averaged wind in the BL.  This prediction can be misleading if there is a large change in wind direction through the BL."),
  },
  {
    _T("hbl"),
    N_("H bl"),
    N_("Height of the top of the mixing layer, which for thermal convection is the average top of a dry thermal.  Over flat terrain, maximum thermalling heights will be lower due to the glider descent rate and other factors.  In the presence of clouds (which release additional buoyancy aloft, creating \"cloudsuck\") the updraft top will be above this forecast, but the maximum thermalling height will then be limited by the cloud base.  Further, when the mixing results from shear turbulence rather than thermal mixing this parameter is not useful for glider flying. "),
  },
  {
    _T("dwcrit"),
    N_("dwcrit"),
    N_("This parameter estimates the height above ground at which the average dry updraft strength drops below 225 fpm and is expected to give better quantitative numbers for the maximum cloudless thermalling height than the BL Top height, especially when mixing results from vertical wind shear rather than thermals.  (Note: the present assumptions tend to underpredict the max. thermalling height for dry consitions.) In the presence of clouds the maximum thermalling height may instead be limited by the cloud base.  Being for \"dry\" thermals, this parameter omits the effect of \"cloudsuck\"."),
  },
  {
    _T("blcloudpct"),
    N_("bl cloud"),
    N_("This parameter provides an additional means of evaluating the formation of clouds within the BL and might be used either in conjunction with or instead of the other cloud prediction parameters.  It assumes a very simple relationship between cloud cover percentage and the maximum relative humidity within the BL.  The cloud base height is not predicted, but is expected to be below the BL Top height."),
  },
  {
    _T("sfctemp"),
    N_("Sfc temp"),
    N_("The temperature at a height of 2m above ground level.  This can be compared to observed surface temperatures as an indication of model simulation accuracy; e.g. if observed surface temperatures are significantly below those forecast, then soaring conditions will be poorer than forecast."),
  },
  {
    _T("hwcrit"),
    N_("hwcrit"),
    N_("This parameter estimates the height at which the average dry updraft strength drops below 225 fpm and is expected to give better quantitative numbers for the maximum cloudless thermalling height than the BL Top height, especially when mixing results from vertical wind shear rather than thermals.  (Note: the present assumptions tend to underpredict the max. thermalling height for dry consitions.) In the presence of clouds the maximum thermalling height may instead be limited by the cloud base.  Being for \"dry\" thermals, this parameter omits the effect of \"cloudsuck\"."),
  },
  {
    _T("wblmaxmin"),
    N_("wblmaxmin"),
    N_("Maximum grid-area-averaged extensive upward or downward motion within the BL as created by horizontal wind convergence. Positive convergence is associated with local small-scale convergence lines.  Negative convergence (divergence) produces subsiding vertical motion, creating low-level inversions which limit thermalling heights."),
  },
  {
    _T("blcwbase"),
    N_("blcwbase"),
    NULL,
  },
};

RasterWeather::RasterWeather()
  :center(Angle::native(fixed_zero), Angle::native(fixed_zero)),
    _parameter(0),
    _weather_time(0),
    reload(true),
    weather_map(NULL)
{
  for (unsigned i = 0; i < MAX_WEATHER_TIMES; i++)
    weather_available[i] = false;
}

RasterWeather::~RasterWeather() 
{
  Close();
}

int
RasterWeather::IndexToTime(int index)
{
  if (index % 2 == 0) {
    return (index / 2) * 100;
  } else {
    return (index / 2) * 100 + 30;
  }
}

void
RasterWeather::SetParameter(unsigned i)
{
  Poco::ScopedRWLock protect(lock, true);
  _parameter = i;
  reload = true;
}

void
RasterWeather::SetTime(unsigned i)
{
  Poco::ScopedRWLock protect(lock, true);
  _weather_time = i;
  reload = true;
}

const RasterMap *
RasterWeather::GetMap() const
{
  // JMW this is not safe in TerrainRenderer's use
  Poco::ScopedRWLock protect(lock, false);
  return weather_map;
}

unsigned
RasterWeather::GetParameter() const
{
  Poco::ScopedRWLock protect(lock, false);
  return _parameter;
}

unsigned
RasterWeather::GetTime() const
{
  Poco::ScopedRWLock protect(lock, false);
  return _weather_time;
}

bool
RasterWeather::isWeatherAvailable(unsigned t) const
{
  Poco::ScopedRWLock protect(lock, false);
  assert(t < MAX_WEATHER_TIMES);
  return weather_available[min((unsigned)MAX_WEATHER_TIMES, t - 1)];
}

void
RasterWeather::GetFilename(TCHAR *rasp_filename, const TCHAR* name,
                           unsigned time_index)
{
  TCHAR fname[MAX_PATH];
  _stprintf(fname, _T("xcsoar-rasp.dat/%s.curr.%04dlst.d2.jp2"), name,
            IndexToTime(time_index));
  LocalPath(rasp_filename, fname);
}

bool
RasterWeather::LoadItem(const TCHAR* name, unsigned time_index)
{
  TCHAR rasp_filename[MAX_PATH];
  GetFilename(rasp_filename, name, time_index);
  RasterMap *map = new RasterMap(rasp_filename, NULL, NULL);
  if (!map->isMapLoaded()) {
    delete map;
    return false;
  }

  delete weather_map;
  weather_map = map;
  return true;
}

bool
RasterWeather::ExistsItem(const TCHAR* name, unsigned time_index) const
{
  TCHAR rasp_filename[MAX_PATH];
  GetFilename(rasp_filename, name, time_index);

  ZZIP_FILE *zf = zzip_fopen(NarrowPathName(rasp_filename), "rb");
  if (zf == NULL)
    return false;

  zzip_fclose(zf);
  return true;
}

void
RasterWeather::ScanAll(const GeoPoint &location)
{
  /* not holding the lock here, because this method is only called
     during startup, when the other threads aren't running yet */

  TCHAR fname[MAX_PATH];
  LocalPath(fname, _T("xcsoar-rasp.dat"));
  if (!File::Exists(fname))
    return;

  ProgressGlue::SetRange(MAX_WEATHER_TIMES);
  for (unsigned i = 0; i < MAX_WEATHER_TIMES; i++) {
    ProgressGlue::SetValue(i);
    weather_available[i] = ExistsItem(_T("wstar"), i) ||
      ExistsItem(_T("wstar_bsratio"), i);
  }
}

void
RasterWeather::Reload(int day_time)
{
  static unsigned last_weather_time;
  bool found = false;
  bool now = false;

  if (_parameter == 0)
    // will be drawing terrain
    return;

  Poco::ScopedRWLock protect(lock, true);
  if (_weather_time == 0) {
    // "Now" time, so find time in half hours
    unsigned half_hours = (TimeLocal(day_time) / 1800) % 48;
    _weather_time = max(_weather_time, half_hours);
    now = true;
  }

  // limit values, for safety
  _weather_time = min(MAX_WEATHER_TIMES - 1, _weather_time);
  if (_weather_time != last_weather_time)
    reload = true;

  if (!reload) {
    // no change, quick exit.
    if (now)
      // must return to 0 = Now time on exit
      _weather_time = 0;

    return;
  }

  reload = false;

  last_weather_time = _weather_time;

  // scan forward to next valid time
  while ((_weather_time < MAX_WEATHER_TIMES) && (!found)) {
    if (!weather_available[_weather_time]) {
      _weather_time++;
    } else {
      found = true;

      _Close();

      if (!LoadItem(WeatherDescriptors[_parameter].name, _weather_time) &&
          _parameter == 1)
        LoadItem(_T("wstar_bsratio"), _weather_time);
    }
  }

  // can't find valid time, so reset to zero
  if (!found || now)
    _weather_time = 0;
}

void
RasterWeather::Close()
{
  Poco::ScopedRWLock protect(lock, true);
  _Close();
}

void
RasterWeather::_Close()
{
  delete weather_map;
  weather_map = NULL;
  center = GeoPoint(Angle::native(fixed_zero), Angle::native(fixed_zero));
}

void
RasterWeather::SetViewCenter(const GeoPoint &location)
{
  Poco::ScopedRWLock protect(lock, true);

  /* only update the RasterMap if the center was moved far enough */
  if (Distance(center, location) < fixed(1000))
    return;

  center = location;

  if (weather_map != NULL)
    weather_map->SetViewCenter(location);
}

const TCHAR*
RasterWeather::ItemLabel(unsigned i)
{
  if (gcc_unlikely(i >= MAX_WEATHER_MAP))
    return NULL;

  const TCHAR *label = WeatherDescriptors[i].label;
  if (gcc_unlikely(label == NULL))
    return NULL;

  return gettext(label);
}

const TCHAR*
RasterWeather::ItemHelp(unsigned i)
{
  if (gcc_unlikely(i >= MAX_WEATHER_MAP))
    return NULL;

  const TCHAR *help = WeatherDescriptors[i].help;
  if (gcc_unlikely(help == NULL))
    return NULL;

  return gettext(help);
}

void
RasterWeather::ValueToText(TCHAR* Buffer, short val) const
{
  *Buffer = _T('\0');

  switch (_parameter) {
  case 0:
    return;
  case 1: // wstar
    _stprintf(Buffer, _T("%.1f%s"), (double)
              Units::ToUserUnit(fixed(val - 200) / 100, Units::VerticalSpeedUnit),
              Units::GetVerticalSpeedName());
    return;
  case 2: // blwindspd
    _stprintf(Buffer, _T("%.0f%s"), (double)
              Units::ToUserUnit(fixed(val) / 100, Units::SpeedUnit),
              Units::GetSpeedName());
    return;
  case 3: // hbl
    _stprintf(Buffer, _T("%.0f%s"), (double)
              Units::ToUserUnit(fixed(val), Units::AltitudeUnit),
              Units::GetAltitudeName());
    return;
  case 4: // dwcrit
    _stprintf(Buffer, _T("%.0f%s"), (double)
              Units::ToUserUnit(fixed(val), Units::AltitudeUnit),
              Units::GetAltitudeName());
    return;
  case 5: // blcloudpct
    _stprintf(Buffer, _T("%d%%"), (int)max(0, min(100, (int)val)));
    return;
  case 6: // sfctemp
    _stprintf(Buffer, _T("%d")_T(DEG), val / 2 - 20);
    return;
  case 7: // hwcrit
    _stprintf(Buffer, _T("%.0f%s"), (double)
              Units::ToUserUnit(fixed(val), Units::AltitudeUnit),
              Units::GetAltitudeName());
    return;
  case 8: // wblmaxmin
    _stprintf(Buffer, _T("%.1f%s"), (double)
              Units::ToUserUnit(fixed(val - 200) / 100, Units::VerticalSpeedUnit),
              Units::GetVerticalSpeedName());
    return;
  case 9: // blcwbase
    _stprintf(Buffer, _T("%.0f%s"), (double)
              Units::ToUserUnit(fixed(val), Units::AltitudeUnit),
              Units::GetAltitudeName());
    return;
  default:
    // error!
    break;
  }
}
