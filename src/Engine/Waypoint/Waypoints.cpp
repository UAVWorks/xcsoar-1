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

#include "Waypoints.hpp"
#include "WaypointVisitor.hpp"
#include "StringUtil.hpp"

// global, used for test harness
unsigned n_queries = 0;
extern long count_intersections;

/**
 * Container accessor to allow a WaypointVisitor to visit WaypointEnvelopes 
 */
class WaypointEnvelopeVisitor {
public:
  /**
   * Constructor
   *
   * @param wve Contained visitor
   *
   * @return Initialised object
   */
  WaypointEnvelopeVisitor(WaypointVisitor* wve):waypoint_visitor(wve) {};

  /**
   * Accessor operator to perform visit
   */
  void
  operator()(const WaypointEnvelope& as)
  {
    Visit(as);
  }

  /**
   * Visit item inside envelope
   */
  void
  Visit(const WaypointEnvelope& as)
  {
    waypoint_visitor->Visit(as.get_waypoint());
  }

private:
  WaypointVisitor *waypoint_visitor;
};

Waypoints::Waypoints():
  next_id(1),
  m_file0_writable(false),
  m_home(NULL)
{
}

void
Waypoints::optimise()
{
  if (task_projection.update_fast()) {
    // task projection changed, so need to push items back onto stack
    std::copy(begin(), end(), std::back_inserter(tmp_wps));
    name_tree.clear();
    waypoint_tree.clear();
  }

  if (!tmp_wps.empty()) {
    while (!tmp_wps.empty()) {
      WaypointEnvelope &w = tmp_wps.front();
      w.project(task_projection);
      WaypointTree::iterator it = waypoint_tree.insert(w);
      tmp_wps.pop_front();
    }
    waypoint_tree.optimize();

    /* rebuild the radix tree - we have to do this because the
       pointers to Waypoint objects in KDTree are not stable */
    name_tree.clear();
    for (Waypoints::WaypointTree::const_iterator it = begin();
         it != end(); ++it) {
      const Waypoint &wp = it->get_waypoint();
      TCHAR normalized_name[wp.Name.length() + 1];
      normalize_search_string(normalized_name, wp.Name.c_str());
      name_tree.add(normalized_name, &wp);
    }
  }
}

void
Waypoints::append(Waypoint& wp)
{
  if (empty())
    task_projection.reset(wp.Location);

  task_projection.scan_location(wp.Location);
  wp.id = next_id++;
  tmp_wps.push_back(WaypointEnvelope(wp));
}


const Waypoint*
Waypoints::get_nearest(const GeoPoint &loc) const 
{
  WaypointTree::const_iterator it = find_nearest(loc);

  if (it != waypoint_tree.end())
    return &it->get_waypoint();

  return NULL;
}

Waypoints::WaypointTree::const_iterator
Waypoints::find_nearest(const GeoPoint &loc) const
{
  WaypointEnvelope bb_target(loc, task_projection);
  std::pair<WaypointTree::const_iterator, double> found =
      waypoint_tree.find_nearest(bb_target);

#ifdef INSTRUMENT_TASK
  n_queries++;
#endif

  return found.first;
}

void
Waypoints::set_details(const Waypoint& wp, const tstring& Details)
{
  WaypointTree::iterator found = waypoint_tree.begin();
  while (found != waypoint_tree.end()) {
    if (found->get_waypoint().id == wp.id)
      found->set_details(Details);

    ++found;
  }
}

const Waypoint*
Waypoints::lookup_name(const TCHAR *name) const
{
  TCHAR normalized[_tcslen(name) + 1];
  normalize_search_string(normalized, name);
  return name_tree.get(normalized, NULL);
}

const Waypoint*
Waypoints::lookup_location(const GeoPoint &loc, const fixed range) const
{
  WaypointEnvelope bb_target(loc, task_projection);
  std::pair<WaypointTree::const_iterator, double> found =
      waypoint_tree.find_nearest(bb_target);

#ifdef INSTRUMENT_TASK
  n_queries++;
#endif

  if (found.first != waypoint_tree.end()) {
    const Waypoint* wp = &(found.first)->get_waypoint();
    if (wp->Location == loc)
      return wp;
    else if (positive(range) && (wp->is_close_to(loc, range)))
      return wp;
  }

  return NULL;
}

const Waypoint*
Waypoints::find_home() const
{
  if (m_home && m_home->Flags.Home)
    return m_home;

  for (WaypointTree::const_iterator found = waypoint_tree.begin();
       found != waypoint_tree.end(); ++found) {
    const Waypoint* wp = &(*found).get_waypoint();
    if (wp->Flags.Home) {
      m_home = wp;
      return wp;
    }
  }

  return NULL;
}

bool
Waypoints::set_home(const unsigned id)
{
  m_home = NULL;

  for (WaypointTree::iterator found = waypoint_tree.begin();
       found != waypoint_tree.end(); ++found) {
    const WaypointEnvelope* wp = &(*found);

    if (wp->get_waypoint().id == id) {
      m_home = &wp->get_waypoint();
      wp->set_home(true);
      return true;
    }
  }

  return false;
}

const Waypoint*
Waypoints::lookup_id(const unsigned id) const
{
  for (WaypointTree::const_iterator found = waypoint_tree.begin();
       found != waypoint_tree.end(); ++found)
    if (found->get_waypoint().id == id)
      return &found->get_waypoint();

  return NULL;
}


Waypoints::WaypointTree::const_iterator
Waypoints::find_id(const unsigned id) const
{
  WaypointTree::const_iterator found = waypoint_tree.begin();

  while (found != waypoint_tree.end()) {
    if (found->get_waypoint().id == id) {
      break;
    }
    ++found;
  }

#ifdef INSTRUMENT_TASK
  n_queries++;
#endif

  return found;
}

std::vector<WaypointEnvelope>
Waypoints::find_within_range(const GeoPoint &loc, const fixed range) const
{
  WaypointEnvelope bb_target(loc, task_projection);
  const unsigned mrange = task_projection.project_range(loc, range);

  std::vector<WaypointEnvelope> vectors;
  waypoint_tree.find_within_range(bb_target, mrange, std::back_inserter(vectors));

#ifdef INSTRUMENT_TASK
  n_queries++;
#endif

  return vectors;
}

void
Waypoints::visit_within_range(const GeoPoint &loc, const fixed range,
    WaypointVisitor& visitor) const
{
  WaypointEnvelope bb_target(loc, task_projection);
  const unsigned mrange = task_projection.project_range(loc, range);

  WaypointEnvelopeVisitor wve(&visitor);

  waypoint_tree.visit_within_range(bb_target, mrange, wve);

#ifdef INSTRUMENT_TASK
  n_queries++;
#endif
}

/**
 * Forwards a visited way point to another visitor if it is within the
 * specified radius.
 */
class RadiusVisitor {
  FlatGeoPoint location;
  unsigned mrange;
  WaypointVisitor *visitor;

public:
  RadiusVisitor(FlatGeoPoint _location, unsigned _mrange,
                WaypointVisitor *_visitor)
    :location(_location), mrange(_mrange), visitor(_visitor) {}

  void operator()(const WaypointEnvelope &envelope) {
    Visit(envelope);
  }

  void Visit(const WaypointEnvelope &envelope) {
#ifdef INSTRUMENT_TASK
    count_intersections++;
#endif

    if (envelope.flat_distance_to(location) <= mrange)
      (*visitor)(envelope.get_waypoint());
  }
};

void
Waypoints::visit_within_radius(const GeoPoint &loc, const fixed range,
    WaypointVisitor& visitor) const
{
  const unsigned mrange = task_projection.project_range(loc, range);
  WaypointEnvelope bb_target(loc, task_projection);

  FlatGeoPoint floc = task_projection.project(loc);
  RadiusVisitor radius_visitor(floc, mrange, &visitor);
  waypoint_tree.visit_within_range(bb_target, mrange, radius_visitor);
}

struct VisitorAdapter {
  WaypointVisitor &visitor;
  VisitorAdapter(WaypointVisitor &_visitor):visitor(_visitor) {}

  void operator()(const Waypoint *wp) {
    visitor.Visit(*wp);
  }
};

void
Waypoints::visit_name_prefix(const TCHAR *prefix,
                             WaypointVisitor& visitor) const
{
  TCHAR normalized[_tcslen(prefix) + 1];
  normalize_search_string(normalized, prefix);
  VisitorAdapter adapter(visitor);
  name_tree.visit_prefix(normalized, adapter);
}

Waypoints::WaypointTree::const_iterator
Waypoints::begin() const
{
  return waypoint_tree.begin();
}

Waypoints::WaypointTree::const_iterator
Waypoints::end() const
{
  return waypoint_tree.end();
}

void
Waypoints::clear()
{
  m_home = NULL;
  name_tree.clear();
  waypoint_tree.clear();
  next_id = 1;
  m_file0_writable = false;
}

unsigned
Waypoints::size() const
{
  return waypoint_tree.size();
}

bool
Waypoints::empty() const
{
  return waypoint_tree.empty() && tmp_wps.empty();
}

void
Waypoints::erase(const Waypoint& wp)
{
  if (m_home != NULL && m_home->id == wp.id)
    m_home = NULL;

  WaypointEnvelope w(wp);
  w.project(task_projection);

  WaypointTree::const_iterator it = waypoint_tree.find_exact(w);
  name_tree.remove(it->get_waypoint().Name.c_str(), &it->get_waypoint());
  waypoint_tree.erase(it);
}

void
Waypoints::replace(const Waypoint& orig, Waypoint& replacement)
{
  erase(orig);

  /* preserve the old waypoint id */
  replacement.id = orig.id;
  tmp_wps.push_back(WaypointEnvelope(replacement));
}

Waypoint
Waypoints::create(const GeoPoint &location)
{
  Waypoint edit_waypoint(location);

  if (empty()) {
    // first waypoint, put into primary file (this will be auto-generated)
    edit_waypoint.FileNum = 0;
    m_file0_writable = true;
  } else if (m_file0_writable) {
    edit_waypoint.FileNum = 0;
  } else {
    edit_waypoint.FileNum = 1;
  }
  return edit_waypoint;
}

void
Waypoints::set_file0_writable(const bool set)
{
  m_file0_writable = set;
}

bool
Waypoints::get_writable(const Waypoint& wp) const
{
  if (wp.FileNum == 0) {
    return m_file0_writable || empty();
  } else {
    return false;
  }
}


bool
Waypoints::find_duplicate(Waypoint& waypoint)
{
  const Waypoint* found = lookup_name(waypoint.Name);
  if (found && found->is_close_to(waypoint.Location, fixed(100))) {
    waypoint = *found;
    return true;
  }
  append(waypoint);
  return false;
}
