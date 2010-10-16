
#include "test_debug.hpp"
#include "Atmosphere/Pressure.hpp"

static bool
test_find_qnh()
{
  AtmosphericPressure pres;
  pres.FindQNH(fixed(100), fixed(100));
  return fabs(pres.get_QNH()-fixed(1013.25))<fixed(0.01);
}

static bool
test_find_qnh2()
{
  AtmosphericPressure pres;
  pres.FindQNH(fixed(100), fixed(120));
  if (verbose) {
    printf("%g\n",FIXED_DOUBLE(pres.get_QNH()));
  }
  return fabs(pres.get_QNH()-fixed(1015.6))<fixed(0.1);
  // example, QNH=1014, ps=100203
  // alt= 100
  // alt_known = 120
  // qnh= 1016
}

static bool
test_qnh_to_static()
{
  AtmosphericPressure pres;
  fixed p0 = pres.QNHAltitudeToStaticPressure(fixed_zero);
  if (verbose) {
    printf("%g\n",FIXED_DOUBLE(p0));
  }
  return fabs(p0-fixed(101325))<fixed(0.1);
}

static bool
test_qnh_round()
{
  AtmosphericPressure pres;
  pres.FindQNH(fixed(100), fixed(120));
  fixed h0 = pres.AltitudeToQNHAltitude(fixed(100));
  if (verbose) {
    printf("%g\n",FIXED_DOUBLE(h0));
  }
  return fabs(h0-fixed(120))<fixed(1);
}

static bool
test_qnh_round2()
{
  AtmosphericPressure pres;
  pres.FindQNH(fixed(100), fixed(120));
  fixed p0 = pres.get_QNH()*100;
  fixed h0 = pres.StaticPressureToQNHAltitude(p0);
  if (verbose) {
    printf("%g %g\n",FIXED_DOUBLE(p0),FIXED_DOUBLE(h0));
  }
  return fabs(h0)<fixed(1);
}

static bool
test_isa_pressure(const fixed alt, const fixed prat)
{
  AtmosphericPressure pres;
  fixed p0 = pres.QNHAltitudeToStaticPressure(alt);
  if (verbose) {
    printf("%g\n",FIXED_DOUBLE(p0));
  }
  return fabs(p0/fixed(101325)-prat)<fixed(0.001);
}

static bool
test_isa_density(const fixed alt, const fixed prat)
{
  fixed p0 = AtmosphericPressure::AirDensity(alt);
  if (verbose) {
    printf("%g\n",FIXED_DOUBLE(p0));
  }
  return fabs(p0/fixed(1.225)-prat)<fixed(0.001);
}

int main(int argc, char** argv)
{

  if (!parse_args(argc,argv)) {
    return 0;
  }

  plan_tests(9);

  ok(test_find_qnh(),"find qnh 0-0",0);
  ok(test_find_qnh2(),"find qnh 100-120",0);

  ok(test_qnh_to_static(),"qnh to static",0);
  ok(test_qnh_round(),"qnh round trip",0);

  ok(test_qnh_round2(),"qnh round 2",0);

  ok(test_isa_pressure(fixed(1524), fixed(0.8320)), "isa pressure at 1524m",0);
  ok(test_isa_pressure(fixed(6096), fixed(0.4594)), "isa pressure at 6096m",0);

  ok(test_isa_density(fixed(1524), fixed(0.8617)), "isa density at 1524m",0);
  ok(test_isa_density(fixed(6096), fixed(0.5328)), "isa density at 6096m",0);

  return exit_status();
}
