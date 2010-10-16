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
#include "Units.hpp"
#include "Device/device.hpp"
#include "Math/FastMath.h"
#include "DataField/Boolean.hpp"
#include "MainWindow.hpp"
#include "PeriodClock.hpp"

static WndForm *wf=NULL;

static void OnCloseClicked(WindowControl * Sender){
	(void)Sender;
  wf->SetModalResult(mrOK);
}

static fixed VegaDemoW = fixed_zero;
static fixed VegaDemoV = fixed_zero;
static bool VegaDemoAudioClimb = true;


static void VegaWriteDemo(void) {
  static PeriodClock last_time;
  if (!last_time.check_update(250))
    return;

  TCHAR dbuf[100];
  _stprintf(dbuf, _T("PDVDD,%d,%d"),
            iround(VegaDemoW * 10),
            iround(VegaDemoV * 10));
  VarioWriteNMEA(dbuf);
};



static void OnVegaDemoW(DataField *Sender,
			 DataField::DataAccessKind_t Mode){
  switch(Mode){
    case DataField::daChange:
      VegaDemoW = Units::ToSysVSpeed(Sender->GetAsFixed());
      VegaWriteDemo();
    break;
  }
}


static void OnVegaDemoV(DataField *Sender,
			 DataField::DataAccessKind_t Mode){
  switch(Mode){
    case DataField::daChange:
      VegaDemoV = Units::ToSysSpeed(Sender->GetAsFixed());
      VegaWriteDemo();
    break;
  }
}


static void OnVegaDemoAudioClimb(DataField *Sender,
			 DataField::DataAccessKind_t Mode){
  switch(Mode){
    case DataField::daChange:
      VegaDemoAudioClimb = (Sender->GetAsInteger()==1);
      VegaWriteDemo();
    break;
  }
}


static CallBackTableEntry CallBackTable[]={
  DeclareCallBackEntry(OnVegaDemoW),
  DeclareCallBackEntry(OnVegaDemoV),
  DeclareCallBackEntry(OnVegaDemoAudioClimb),
  DeclareCallBackEntry(OnCloseClicked),
  DeclareCallBackEntry(NULL)
};


void dlgVegaDemoShowModal(void){
  wf = LoadDialog(CallBackTable,
		      XCSoarInterface::main_window,
		      _T("IDR_XML_VEGADEMO"));

  WndProperty* wp;

  if (!wf) return;

  VarioWriteNMEA(_T("PDVSC,S,DemoMode,0"));
  VarioWriteNMEA(_T("PDVSC,S,DemoMode,3"));

  wp = (WndProperty*)wf->FindByName(_T("prpVegaDemoW"));
  if (wp) {
    wp->GetDataField()->SetAsFloat(Units::ToUserVSpeed(fixed(VegaDemoW)));
    wp->GetDataField()->SetUnits(Units::GetVerticalSpeedName());
    wp->RefreshDisplay();
  }

  wp = (WndProperty*)wf->FindByName(_T("prpVegaDemoV"));
  if (wp) {
    wp->GetDataField()->SetAsFloat(Units::ToUserVSpeed(fixed(VegaDemoV)));
    wp->GetDataField()->SetUnits(Units::GetSpeedName());
    wp->RefreshDisplay();
  }

  wp = (WndProperty*)wf->FindByName(_T("prpVegaDemoAudioClimb"));
  if (wp) {
    DataFieldBoolean *df = (DataFieldBoolean *)wp->GetDataField();
    df->Set(VegaDemoAudioClimb);
    wp->RefreshDisplay();
  }

  wf->ShowModal();

  // deactivate demo.
  VarioWriteNMEA(_T("PDVSC,S,DemoMode,0"));

  delete wf;

  wf = NULL;

}

