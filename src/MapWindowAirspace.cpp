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

#include "MapWindow.hpp"
#include "MapCanvas.hpp"
#include "Airspace/Airspaces.hpp"
#include "Airspace/AirspacePolygon.hpp"
#include "Airspace/AirspaceCircle.hpp"
#include "Airspace/AirspaceVisitor.hpp"
#include "Airspace/AirspaceVisibility.hpp"
#include "Airspace/AirspaceWarning.hpp"
#include "Airspace/AirspaceWarningVisitor.hpp"
#include "Airspace/ProtectedAirspaceWarningManager.hpp"
#include "MapDrawHelper.hpp"
#include "Screen/Layout.hpp"
#include "Screen/Graphics.hpp"

class AirspaceWarningCopy: 
  public AirspaceWarningVisitor
{
public:
  void Visit(const AirspaceWarning& as) {
    if (as.get_warning_state()== AirspaceWarning::WARNING_INSIDE) {
      ids_inside.checked_append(&as.get_airspace());
    } else if (as.get_warning_state()> AirspaceWarning::WARNING_CLEAR) {
      ids_warning.checked_append(&as.get_airspace());
      locs.checked_append(as.get_solution().location);
    }
    if (!as.get_ack_expired()) {
      ids_acked.checked_append(&as.get_airspace());
    }
  }

  const StaticArray<GeoPoint,32> &get_locations() const {
    return locs;
  }
  bool is_warning(const AbstractAirspace& as) const {
    return find(as, ids_warning);
  }
  bool is_acked(const AbstractAirspace& as) const {
    return find(as, ids_acked);
  }
  bool is_inside(const AbstractAirspace& as) const {
    return find(as, ids_inside);
  }

private:
  bool find(const AbstractAirspace& as, 
            const StaticArray<const AbstractAirspace *,64> &list) const {
    return list.contains(&as);
  }

  StaticArray<const AbstractAirspace *,64> ids_inside, ids_warning, ids_acked;
  StaticArray<GeoPoint,32> locs;
};


class AirspaceMapVisible: public AirspaceVisible
{
public:
  AirspaceMapVisible(const SETTINGS_COMPUTER& _settings, 
                     const fixed& _altitude, const bool& _border,
                     const AirspaceWarningCopy& warnings):
    AirspaceVisible(_settings, _altitude),
    m_border(_border),
    m_warnings(warnings)
    {
    };

  virtual bool operator()( const AbstractAirspace& airspace ) const { 
    return condition(airspace);
  }

  bool condition( const AbstractAirspace& airspace ) const { 
    return parent_condition(airspace) 
      || m_warnings.is_inside(airspace)
      || m_warnings.is_warning(airspace);
  }
private:
  const bool &m_border;
  const AirspaceWarningCopy& m_warnings;
};

/**
 * Class to render airspaces onto map in two passes,
 * one for border, one for area.
 * This is a bit slow because projections are performed twice.
 * The old way of doing it was possibly faster but required a lot
 * of code overhead.
 */
class AirspaceVisitorMap: 
  public AirspaceVisitor,
  public MapDrawHelper
{
public:
  AirspaceVisitorMap(MapDrawHelper &_helper,
                     const AirspaceWarningCopy& warnings):
    MapDrawHelper(_helper),
    m_warnings(warnings),
    pen_thick(Pen::SOLID, IBLSCALE(10), Color(0x00, 0x00, 0x00)),
    pen_medium(Pen::SOLID, IBLSCALE(3), Color(0x00, 0x00, 0x00)) {
    m_use_stencil = true;
  }

  void Visit(const AirspaceCircle& airspace) {
    if (m_warnings.is_acked(airspace))
      return;

    buffer_render_start();
    set_buffer_pens(airspace);

    POINT center = m_proj.LonLat2Screen(airspace.get_center());
    unsigned radius = m_proj.DistanceMetersToScreen(airspace.get_radius());
    draw_circle(m_buffer, center, radius);
  }

  void Visit(const AirspacePolygon& airspace) {
    if (m_warnings.is_acked(airspace))
      return;

    buffer_render_start();
    set_buffer_pens(airspace);
    draw_search_point_vector(m_buffer, airspace.get_points());
  }

  void draw_intercepts() {
    buffer_render_finish();
  }

private:
  static unsigned char light_color(unsigned char c) {
    return ((c ^ 0xff) >> 1) ^ 0xff;
  }

  /**
   * Returns a lighter version of the specified color, adequate for
   * SRCAND filtering.
   */
  static Color light_color(Color c) {
    return Color(light_color(c.red()), light_color(c.green()),
                 light_color(c.blue()));
  }

  void set_buffer_pens(const AbstractAirspace &airspace) {
    // this color is used as the black bit
    m_buffer.set_text_color(light_color(Graphics::Colours[m_settings_map.
                                        iAirspaceColour[airspace.get_type()]]));

    // get brush, can be solid or a 1bpp bitmap
    m_buffer.select(Graphics::hAirspaceBrushes[m_settings_map.
                                            iAirspaceBrush[airspace.get_type()]]);
    m_buffer.white_pen();

    if (m_warnings.is_warning(airspace) || m_warnings.is_inside(airspace)) {
      m_stencil.black_brush();
      m_stencil.select(pen_medium);
    } else {
      m_stencil.select(pen_thick);
      m_stencil.hollow_brush();
    }

  }

  const AirspaceWarningCopy& m_warnings;
  Pen pen_thick;
  Pen pen_medium;
};

class AirspaceOutlineRenderer : public AirspaceVisitor, protected MapCanvas {
  bool black;

public:
  AirspaceOutlineRenderer(Canvas &_canvas, const Projection &_projection,
                          bool _black)
    :MapCanvas(_canvas, _projection), black(_black) {
    if (black)
      canvas.black_pen();
    canvas.hollow_brush();
  }

protected:
  void setup_canvas(const AbstractAirspace &airspace) {
    if (!black)
      canvas.select(Graphics::hAirspacePens[airspace.get_type()]);
  }

public:
  void Visit(const AirspaceCircle& airspace) {
    setup_canvas(airspace);
    circle(airspace.get_center(), airspace.get_radius());
  }

  void Visit(const AirspacePolygon& airspace) {
    setup_canvas(airspace);
    draw(airspace.get_points());
  }
};

void
MapWindow::DrawAirspaceIntersections(Canvas &canvas) const
{
  for (unsigned i = m_airspace_intersections.size(); i--;) {
    POINT sc;
    if (render_projection.LonLat2ScreenIfVisible(m_airspace_intersections[i], &sc))
      Graphics::hAirspaceInterceptBitmap.draw(canvas, bitmap_canvas, sc.x, sc.y);
  }
}

/**
 * Draws the airspace to the given canvas
 * @param canvas The drawing canvas
 * @param buffer The drawing buffer
 */
void
MapWindow::DrawAirspace(Canvas &canvas, Canvas &buffer)
{
  if (airspace_database == NULL)
    return;

  AirspaceWarningCopy awc;
  if (airspace_warnings != NULL)
    airspace_warnings->visit_warnings(awc);

  MapDrawHelper helper(canvas, buffer, stencil_canvas, render_projection,
                        SettingsMap());
  AirspaceVisitorMap v(helper, awc);
  const AirspaceMapVisible visible(SettingsComputer(),
                                   Basic().GetAltitudeBaroPreferred(),
                                   false, awc);

  // JMW TODO wasteful to draw twice, can't it be drawn once?
  // we are using two draws so borders go on top of everything

  airspace_database->visit_within_range(render_projection.GetPanLocation(),
                                        render_projection.GetScreenDistanceMeters(),
                                        v, visible);
  v.draw_intercepts();

  AirspaceOutlineRenderer outline_renderer(canvas, render_projection,
                                           SettingsMap().bAirspaceBlackOutline);
  airspace_database->visit_within_range(render_projection.GetPanLocation(),
                                        render_projection.GetScreenDistanceMeters(),
                                        outline_renderer, visible);

  m_airspace_intersections = awc.get_locations();
}
