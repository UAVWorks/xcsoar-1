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

#include "Dialogs/Internal.hpp"
#include "Dialogs/Message.hpp"
#include "Blackboard.hpp"
#include "Airspace/AbstractAirspace.hpp"
#include "Airspace/ProtectedAirspaceWarningManager.hpp"
#include "Math/FastMath.h"
#include "Math/Earth.hpp"
#include "MainWindow.hpp"
#include "MapWindow.hpp"
#include "Components.hpp"
#include "Navigation/Geometry/GeoVector.hpp"

#include <assert.h>
#include <stdio.h>

static const AbstractAirspace* airspace;
static WndForm *wf = NULL;

static void
OnAcknowledgeClicked(WindowControl * Sender)
{
  (void)Sender;

  assert(airspace);

  int answer;
  answer = MessageBoxX(airspace->get_name_text(true).c_str(), 
                       _("Acknowledge for day?"),
                       MB_YESNOCANCEL | MB_ICONQUESTION);

  if (answer == IDYES) {
    airspace_warnings.acknowledge_day(*airspace, true);
    wf->SetModalResult(mrOK);
  } else if (answer == IDNO) {
    airspace_warnings.acknowledge_day(*airspace, false);
    wf->SetModalResult(mrOK);
  }
}

static void
OnCloseClicked(WindowControl * Sender)
{
  (void)Sender;
  wf->SetModalResult(mrOK);
}

static CallBackTableEntry CallBackTable[] = {
  DeclareCallBackEntry(OnAcknowledgeClicked),
  DeclareCallBackEntry(OnCloseClicked),
  DeclareCallBackEntry(NULL)
};

static void
SetValues(void)
{
  assert(airspace);

  WndProperty* wp;

  wp = (WndProperty*)wf->FindByName(_T("prpName"));
  if (wp) {
    wp->SetText(airspace->get_name_text(true).c_str());
    wp->RefreshDisplay();
  }

  wp = (WndProperty*)wf->FindByName(_T("prpType"));
  if (wp) {
    wp->SetText(airspace->get_type_text());
    wp->RefreshDisplay();
  }

  wp = (WndProperty*)wf->FindByName(_T("prpTop"));
  if (wp) {
    wp->SetText(airspace->get_top_text().c_str());
    wp->RefreshDisplay();
  }

  wp = (WndProperty*)wf->FindByName(_T("prpBase"));
  if (wp) {
    wp->SetText(airspace->get_base_text().c_str());
    wp->RefreshDisplay();
  }

  wp = (WndProperty*)wf->FindByName(_T("prpRange"));
  if (wp) {
    const GeoPoint &ac_loc = XCSoarInterface::Basic().Location;
    const GeoPoint closest_loc = airspace->closest_point(ac_loc);
    const GeoVector vec(ac_loc, closest_loc);
    TCHAR buf[80];
    _stprintf(buf, _T("%d%s"),
              (int)Units::ToUserUnit(vec.Distance, Units::DistanceUnit),
              Units::GetDistanceName());
    wp->SetText(buf);
    wp->RefreshDisplay();
  }
}


void
dlgAirspaceDetails(const AbstractAirspace& the_airspace)
{
  airspace = &the_airspace;

  wf = LoadDialog(CallBackTable,
                      XCSoarInterface::main_window,
                      _T("IDR_XML_AIRSPACEDETAILS"));
  if (!wf) return;

  SetValues();

  wf->ShowModal();

  delete wf;
  wf = NULL;
}
