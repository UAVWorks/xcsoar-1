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

#include "FlarmTrafficWindow.hpp"
#include "FLARM/Traffic.hpp"
#include "Screen/Layout.hpp"
#include "Screen/Fonts.hpp"
#include "Units.hpp"
#include "Math/Screen.hpp"
#include "Language.hpp"

#include <assert.h>
#include <stdio.h>

const Color FlarmTrafficWindow::hcWarning(0xFF, 0xA2, 0x00);
const Color FlarmTrafficWindow::hcAlarm(0xFF, 0x00, 0x00);
const Color FlarmTrafficWindow::hcStandard(0x00, 0x00, 0x00);
const Color FlarmTrafficWindow::hcPassive(0x99, 0x99, 0x99);
const Color FlarmTrafficWindow::hcSelection(0x00, 0x00, 0xFF);
const Color FlarmTrafficWindow::hcTeam(0x74, 0xFF, 0x00);
const Color FlarmTrafficWindow::hcBackground(0xFF, 0xFF, 0xFF);
const Color FlarmTrafficWindow::hcRadar(0xB0, 0xB0, 0xB0);

FlarmTrafficWindow::FlarmTrafficWindow(unsigned _padding, bool _small)
  :distance(2000),
   selection(-1), warning(-1),
   padding(_padding),
   small(_small),
   direction(Angle::radians(fixed_zero)),
   side_display_type(1)
{
  memset(&data, 0, sizeof(data));
}

bool
FlarmTrafficWindow::WarningMode() const
{
  assert(warning < FLARM_STATE::FLARM_MAX_TRAFFIC);
  assert(warning < 0 || data.FLARM_Traffic[warning].defined());
  assert(warning < 0 || data.FLARM_Traffic[warning].HasAlarm());

  return warning >= 0;
}

bool
FlarmTrafficWindow::on_create()
{
  PaintWindow::on_create();

  hbWarning.set(hcWarning);
  hbAlarm.set(hcAlarm);
  hbSelection.set(hcSelection);
  hbTeam.set(hcTeam);
  hbRadar.set(hcRadar);

  int width = Layout::FastScale(small ? 1 : 2);
  hpWarning.set(width, hcWarning);
  hpAlarm.set(width, hcAlarm);
  hpStandard.set(width, hcStandard);
  hpPassive.set(width, hcPassive);
  hpSelection.set(width, hcSelection);

  hpPlane.set(width, hcRadar);
  hpRadar.set(1, hcRadar);

  hfNoTraffic.set(Fonts::GetStandardFontFace(), Layout::FastScale(24));
  hfLabels.set(Fonts::GetStandardFontFace(), Layout::FastScale(14));
  if (small)
    hfSideInfo.set(Fonts::GetStandardFontFace(), Layout::FastScale(12), true);
  else
    hfSideInfo.set(Fonts::GetStandardFontFace(), Layout::FastScale(18), true);

  return true;
}

bool
FlarmTrafficWindow::on_resize(unsigned width, unsigned height)
{
  PaintWindow::on_resize(width, height);

  // Calculate Radar size
  radius = min(height, width) / 2 - padding;
  radar_mid.x = width / 2;
  radar_mid.y = height / 2;

  return true;
}

void
FlarmTrafficWindow::SetTarget(int i)
{
  assert(i < FLARM_STATE::FLARM_MAX_TRAFFIC);
  assert(i < 0 || data.FLARM_Traffic[i].defined());

  if (selection == i)
    return;

  selection = i;
  invalidate();
}

/**
 * Tries to select the next target, if impossible selection = -1
 */
void
FlarmTrafficWindow::NextTarget()
{
  // If warning is displayed -> prevent selector movement
  if (WarningMode())
    return;

  assert(selection < FLARM_STATE::FLARM_MAX_TRAFFIC);

  const FLARM_TRAFFIC *traffic;
  if (selection >= 0)
    traffic = data.NextTraffic(&data.FLARM_Traffic[selection]);
  else
    traffic = NULL;

  if (traffic == NULL)
    traffic = data.FirstTraffic();

  SetTarget(traffic);
}

/**
 * Tries to select the previous target, if impossible selection = -1
 */
void
FlarmTrafficWindow::PrevTarget()
{
  // If warning is displayed -> prevent selector movement
  if (WarningMode())
    return;

  assert(selection < FLARM_STATE::FLARM_MAX_TRAFFIC);

  const FLARM_TRAFFIC *traffic;
  if (selection >= 0)
    traffic = data.PreviousTraffic(&data.FLARM_Traffic[selection]);
  else
    traffic = NULL;

  if (traffic == NULL)
    traffic = data.LastTraffic();

  SetTarget(traffic);
}

/**
 * Checks whether the selection is still on the valid target and if not tries
 * to select the next one
 */
void
FlarmTrafficWindow::UpdateSelector()
{
  if (small || !data.FLARM_Available)
    SetTarget(-1);
  else if (selection < 0 || (!data.FLARM_Traffic[selection].defined() &&
                             !SelectNearTarget(sc[selection].x, sc[selection].y,
                                               radius * 2)))
    NextTarget();
}

/**
 * Iterates through the traffic array, finds the target with the highest
 * alarm level and saves it to "warning".
 */
void
FlarmTrafficWindow::UpdateWarnings()
{
  const FLARM_TRAFFIC *alert = data.FindMaximumAlert();
  warning = alert != NULL
    ? (int)data.TrafficIndex(alert)
    : - 1;
}

/**
 * This should be called when the radar needs to be repainted
 */
void
FlarmTrafficWindow::Update(Angle new_direction, const FLARM_STATE &new_data,
                           const SETTINGS_TEAMCODE &new_settings)
{
  direction = new_direction;
  fr.SetAngle(-direction);
  data = new_data;
  settings = new_settings;

  UpdateSelector();
  UpdateWarnings();

  invalidate();
}

/**
 * Returns the distance to the own plane in pixels
 * @param d Distance in meters to the own plane
 */
fixed
FlarmTrafficWindow::RangeScale(fixed d) const
{
  d = d / distance;
  return min(d, fixed_one) * radius;
}

/**
 * Paints a "No Traffic" sign on the given canvas
 * @param canvas The canvas to paint on
 */
void
FlarmTrafficWindow::PaintRadarNoTraffic(Canvas &canvas) const
{
  if (small)
    return;

  const TCHAR* str = _("No Traffic");
  canvas.select(hfNoTraffic);
  SIZE ts = canvas.text_size(str);
  canvas.set_text_color(hcStandard);
  canvas.text(radar_mid.x - (ts.cx / 2), radar_mid.y - (radius / 2), str);
}

/**
 * Paints the traffic symbols on the given canvas
 * @param canvas The canvas to paint on
 */
void
FlarmTrafficWindow::PaintRadarTarget(Canvas &canvas,
                                     const FLARM_TRAFFIC &traffic,
                                     unsigned i)
{
  // Save relative East/North
  fixed x, y;
  x = traffic.RelativeEast;
  y = -traffic.RelativeNorth;

  // Calculate the distance in meters
  fixed d = hypot(x, y);

  // Calculate the distance in pixels
  fixed scale = RangeScale(d);

  // Don't display distracting, far away targets in WarningMode
  if (WarningMode() && !traffic.HasAlarm() && scale == fixed(radius))
    return;

  // x and y are not between 0 and 1 (distance will be handled via scale)
  if (positive(d)) {
    x /= d;
    y /= d;
  } else {
    x = fixed_zero;
    y = fixed_zero;
  }

  // Rotate x and y to have a track up display
  FastRotation::Pair p = fr.Rotate(x, y);
  x = fixed(p.first);
  y = fixed(p.second);

  // Calculate screen coordinates
  sc[i].x = radar_mid.x + iround(x * scale);
  sc[i].y = radar_mid.y + iround(y * scale);

  // Set the arrow color depending on alarm level
  switch (traffic.AlarmLevel) {
  case 1:
    canvas.hollow_brush();
    canvas.select(hpWarning);
    canvas.circle(sc[i].x, sc[i].y, Layout::FastScale(small ? 8 : 16));
    canvas.select(hbWarning);
    break;
  case 2:
  case 3:
    canvas.hollow_brush();
    canvas.select(hpAlarm);
    canvas.circle(sc[i].x, sc[i].y, Layout::FastScale(small ? 8 : 16));
    canvas.circle(sc[i].x, sc[i].y, Layout::FastScale(small ? 10 : 19));
    canvas.select(hbAlarm);
    break;
  case 0:
  case 4:
    if (WarningMode()) {
      canvas.hollow_brush();
      canvas.select(hpPassive);
    } else {
      if (!small && static_cast<unsigned> (selection) == i) {
        canvas.select(hpSelection);
        canvas.select(hbSelection);
      } else {
        canvas.hollow_brush();
        if (traffic.Type != FLARM_TRAFFIC::acGlider
            && traffic.Type != FLARM_TRAFFIC::acHangGlider
            && traffic.Type != FLARM_TRAFFIC::acParaGlider)
          canvas.select(hpPassive);
        else
          canvas.select(hpStandard);
      }
      if (settings.TeamFlarmTracking &&
          traffic.ID == settings.TeamFlarmIdTarget) {
        canvas.select(hbTeam);
      }
    }
    break;
  }

  // Create an arrow polygon
  POINT Arrow[5];
  if (small) {
    Arrow[0].x = -3;
    Arrow[0].y = 4;
    Arrow[1].x = 0;
    Arrow[1].y = -5;
    Arrow[2].x = 3;
    Arrow[2].y = 4;
    Arrow[3].x = 0;
    Arrow[3].y = 2;
    Arrow[4].x = -3;
    Arrow[4].y = 4;
  } else {
    Arrow[0].x = -6;
    Arrow[0].y = 8;
    Arrow[1].x = 0;
    Arrow[1].y = -10;
    Arrow[2].x = 6;
    Arrow[2].y = 8;
    Arrow[3].x = 0;
    Arrow[3].y = 5;
    Arrow[4].x = -6;
    Arrow[4].y = 8;
  }

  // Rotate and shift the arrow
  PolygonRotateShift(Arrow, 5, sc[i].x, sc[i].y,
                     traffic.TrackBearing - direction);

  // Draw the polygon
  canvas.polygon(Arrow, 5);

  if (small) {
    if (WarningMode() && !traffic.HasAlarm())
      return;

    const short relalt =
        iround(Units::ToUserAltitude(traffic.RelativeAltitude / 100));

    // if (relative altitude is other than zero)
    if (relalt == 0)
      return;

    // Write the relativ altitude devided by 100 to the Buffer
    TCHAR Buffer[10];
    _stprintf(Buffer, _T("%d"), abs(relalt));

    // Select font
    canvas.background_transparent();
    canvas.select(hfSideInfo);
    canvas.set_text_color(Color::BLACK);

    // Calculate size of the output string
    SIZE tsize = canvas.text_size(Buffer);

    int dist = Layout::FastScale(traffic.HasAlarm() ? 12 : 8);

    // Draw string
    canvas.text(sc[i].x + dist,
                sc[i].y - tsize.cy / 2,
                Buffer);

    // Set black brush for the up/down arrow
    canvas.black_brush();
    canvas.null_pen();

    // Prepare the triangular polygon
    POINT triangle[4];
    triangle[0].x = 0;
    triangle[0].y = -4;
    triangle[1].x = 3;
    triangle[1].y = 0;
    triangle[2].x = -3;
    triangle[2].y = 0;

    // Flip = -1 for arrow pointing downwards
    short flip = 1;
    if (relalt < 0)
      flip = -1;

    // Shift the arrow to the right position
    for (int j = 0; j < 3; j++) {
      triangle[j].x = Layout::FastScale(triangle[j].x);
      triangle[j].y = Layout::FastScale(triangle[j].y);

      triangle[j].x = sc[i].x + dist + triangle[j].x + tsize.cx / 2;
      triangle[j].y = sc[i].y + flip * (triangle[j].y  - tsize.cy / 2);
    }
    triangle[3].x = triangle[0].x;
    triangle[3].y = triangle[0].y;

    // Draw the arrow
    canvas.polygon(triangle, 4);

    return;
  }

  // if warning exists -> don't draw vertical speeds
  if (WarningMode())
    return;

  // if vertical speed to small or negative -> skip this one
  if (side_display_type == 1 && (traffic.Average30s < fixed(0.5)
        || (traffic.Type != FLARM_TRAFFIC::acGlider
            && traffic.Type != FLARM_TRAFFIC::acHangGlider
            && traffic.Type != FLARM_TRAFFIC::acParaGlider)))
      return;

  // Select font
  canvas.background_transparent();
  canvas.select(hfSideInfo);

  // Format string
  TCHAR tmp[10];

  if (side_display_type == 1)
    Units::FormatUserVSpeed(traffic.Average30s, tmp, 10, false);
  else
    Units::FormatUserArrival(traffic.RelativeAltitude, tmp, 10, true);

  SIZE sz = canvas.text_size(tmp);

  // Draw vertical speed shadow
  canvas.set_text_color(Color::WHITE);
  canvas.text(sc[i].x + Layout::FastScale(11) + 1,
              sc[i].y - sz.cy / 2 + 1, tmp);
  canvas.text(sc[i].x + Layout::FastScale(11) - 1,
              sc[i].y - sz.cy / 2 - 1, tmp);

  // Select color
  if (static_cast<unsigned> (selection) == i)
    canvas.set_text_color(hcSelection);
  else
    canvas.set_text_color(hcStandard);

  // Draw vertical speed
  canvas.text(sc[i].x + Layout::FastScale(11), sc[i].y - sz.cy / 2, tmp);
}

/**
 * Paints the traffic symbols on the given canvas
 * @param canvas The canvas to paint on
 */
void
FlarmTrafficWindow::PaintRadarTraffic(Canvas &canvas)
{
  if (!data.FLARM_Available || data.GetActiveTrafficCount() == 0) {
    PaintRadarNoTraffic(canvas);
    return;
  }

  // Iterate through the traffic (normal traffic)
  for (unsigned i = 0; i < FLARM_STATE::FLARM_MAX_TRAFFIC; ++i) {
    const FLARM_TRAFFIC &traffic = data.FLARM_Traffic[i];

    if (traffic.defined() && !traffic.HasAlarm() &&
        static_cast<unsigned> (selection) != i)
      PaintRadarTarget(canvas, traffic, i);
  }

  if (selection >= 0 && selection < FLARM_STATE::FLARM_MAX_TRAFFIC) {
    const FLARM_TRAFFIC &traffic = data.FLARM_Traffic[selection];

    if (traffic.defined() && !traffic.HasAlarm())
      PaintRadarTarget(canvas, traffic, selection);
  }

  if (!WarningMode())
    return;

  // Iterate through the traffic (alarm traffic)
  for (unsigned i = 0; i < FLARM_STATE::FLARM_MAX_TRAFFIC; ++i) {
    const FLARM_TRAFFIC &traffic = data.FLARM_Traffic[i];

    if (traffic.defined() && traffic.HasAlarm())
      PaintRadarTarget(canvas, traffic, i);
  }
}

/**
 * Paint a plane symbol in the middle of the radar on the given canvas
 * @param canvas The canvas to paint on
 */
void
FlarmTrafficWindow::PaintRadarPlane(Canvas &canvas) const
{
  canvas.select(hpPlane);
  canvas.line(radar_mid.x + Layout::FastScale(small ? 5 : 10),
              radar_mid.y - Layout::FastScale(small ? 1 : 2),
              radar_mid.x - Layout::FastScale(small ? 5 : 10),
              radar_mid.y - Layout::FastScale(small ? 1 : 2));
  canvas.line(radar_mid.x,
              radar_mid.y - Layout::FastScale(small ? 3 : 6),
              radar_mid.x,
              radar_mid.y + Layout::FastScale(small ? 3 : 6));
  canvas.line(radar_mid.x + Layout::FastScale(small ? 2 : 4),
              radar_mid.y + Layout::FastScale(small ? 2 : 4),
              radar_mid.x - Layout::FastScale(small ? 2 : 4),
              radar_mid.y + Layout::FastScale(small ? 2 : 4));
}

/**
 * Paints the radar circle on the given canvas
 * @param canvas The canvas to paint on
 */
void
FlarmTrafficWindow::PaintNorth(Canvas &canvas) const
{
  FastRotation::Pair p = fr.Rotate(fixed_zero, fixed_minus_one);
  fixed x = p.first, y = p.second;

  canvas.set_text_color(Color::WHITE);
  canvas.select(hpRadar);
  canvas.select(hbRadar);
  canvas.background_transparent();
  canvas.select(hfLabels);

  SIZE s = canvas.text_size(_T("N"));
  canvas.circle(radar_mid.x + iround(x * radius),
                radar_mid.y + iround(y * radius), s.cy * 13 / 20);
  canvas.text(radar_mid.x + iround(x * radius) - s.cx / 2,
              radar_mid.y + iround(y * radius) - s.cy / 2, _T("N"));
}

/**
 * Paints the radar circle on the given canvas
 * @param canvas The canvas to paint on
 */
void
FlarmTrafficWindow::PaintRadarBackground(Canvas &canvas) const
{
  canvas.hollow_brush();
  canvas.select(hpRadar);
  canvas.set_text_color(hcRadar);

  // Paint circles
  canvas.circle(radar_mid.x, radar_mid.y, radius);
  canvas.circle(radar_mid.x, radar_mid.y, radius / 2);

  PaintRadarPlane(canvas);

  if (small)
    return;

  // Paint zoom strings
  canvas.select(hfLabels);
  canvas.background_opaque();

  TCHAR distance_string[10];
  Units::FormatUserDistance(distance, distance_string,
                            sizeof(distance_string) / sizeof(distance_string[0]));
  SIZE s = canvas.text_size(distance_string);
  canvas.text(radar_mid.x - s.cx / 2,
              radar_mid.y + radius - s.cy * 3 / 4, distance_string);

  Units::FormatUserDistance(distance / 2, distance_string,
                            sizeof(distance_string) / sizeof(distance_string[0]));
  s = canvas.text_size(distance_string);
  canvas.text(radar_mid.x - s.cx / 2,
              radar_mid.y + radius / 2 - s.cy * 3 / 4, distance_string);

  canvas.background_transparent();

  PaintNorth(canvas);
}

/**
 * This function paints the TrafficRadar onto the given canvas
 * @param canvas The canvas to paint on
 */
void
FlarmTrafficWindow::Paint(Canvas &canvas)
{
  assert(selection < FLARM_STATE::FLARM_MAX_TRAFFIC);
  assert(selection < 0 || data.FLARM_Traffic[selection].defined());
  assert(warning < FLARM_STATE::FLARM_MAX_TRAFFIC);
  assert(warning < 0 || data.FLARM_Traffic[warning].defined());
  assert(warning < 0 || data.FLARM_Traffic[warning].HasAlarm());

  PaintRadarBackground(canvas);
  PaintRadarTraffic(canvas);
}

/**
 * This function is called when the Radar needs repainting.
 * @param canvas The canvas to paint on
 */
void
FlarmTrafficWindow::on_paint(Canvas &canvas)
{
  canvas.clear_white();
  Paint(canvas);
}

bool
FlarmTrafficWindow::SelectNearTarget(int x, int y, int max_distance)
{
  int min_distance = 99999;
  int min_id = -1;

  for (unsigned i = 0; i < FLARM_STATE::FLARM_MAX_TRAFFIC; ++i) {
    // If FLARM target does not exist -> next one
    if (!data.FLARM_Traffic[i].defined())
      continue;

    int distance_sq = (x - sc[i].x) * (x - sc[i].x) +
                      (y - sc[i].y) * (y - sc[i].y);

    if (distance_sq > min_distance
        || distance_sq > max_distance * max_distance)
      continue;

    min_distance = distance_sq;
    min_id = i;
  }

  if (min_id >= 0)
    SetTarget(min_id);

  return min_id >= 0;
}
