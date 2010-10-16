#include "harness_waypoints.hpp"
#include "test_debug.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

/** 
 * Initialises waypoints with random and non-random waypoints
 * for testing
 *
 * @param waypoints waypoints class to add waypoints to
 */
bool setup_waypoints(Waypoints &waypoints, const unsigned n) 
{

  Waypoint wp = waypoints.create(GeoPoint(Angle::degrees(fixed_zero),
                                          Angle::degrees(fixed_zero)));
  wp.Flags.Airport = true;
  wp.Altitude = fixed(0.25);
  waypoints.append(wp);

  wp = waypoints.create(GeoPoint(Angle::degrees(fixed_zero), 
                                 Angle::degrees(fixed_one)));
  wp.Flags.Airport = true;
  wp.Altitude = fixed(0.25);
  waypoints.append(wp);

  wp = waypoints.create(GeoPoint(Angle::degrees(fixed_one), 
                                 Angle::degrees(fixed_one)));
  wp.Name = _T("Hello");
  wp.Flags.Airport = true;
  wp.Altitude = fixed_half;
  waypoints.append(wp);

  wp = waypoints.create(GeoPoint(Angle::degrees(fixed(0.8)), 
                                 Angle::degrees(fixed(0.5))));
  wp.Name = _T("Unk");
  wp.Flags.Airport = true;
  wp.Altitude = fixed(0.25);
  waypoints.append(wp);

  wp = waypoints.create(GeoPoint(Angle::degrees(fixed_one), 
                                 Angle::degrees(fixed_zero)));
  wp.Flags.Airport = true;
  wp.Altitude = fixed(0.25);
  waypoints.append(wp);

  wp = waypoints.create(GeoPoint(Angle::degrees(fixed_zero), 
                                 Angle::degrees(fixed(0.23))));
  wp.Flags.Airport = true;
  wp.Altitude = fixed(0.25);
  waypoints.append(wp);

  for (unsigned i=0; i<(unsigned)std::max((int)n-6,0); i++) {
    int x = rand()%1200-100;
    int y = rand()%1200-100;
    double z = rand()% std::max(terrain_height,1);
    wp = waypoints.create(GeoPoint(Angle::degrees(fixed(x/1000.0)), 
                                   Angle::degrees(fixed(y/1000.0))));
    wp.Flags.Airport = false;
    wp.Altitude = fixed(z);
    waypoints.append(wp);
  }
  waypoints.optimise();

  if (verbose) {
    std::ofstream fin("results/res-wp-in.txt");
    for (unsigned i=1; i<=waypoints.size(); i++) {
      Waypoints::WaypointTree::const_iterator it = waypoints.find_id(i);
      if (it != waypoints.end()) {
#ifdef DO_PRINT
        fin << it->get_waypoint();
#endif
      }
    }
  }
  return true;
}

