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
#include "Screen/Fonts.hpp"
#include "Screen/Layout.hpp"
#include "Gauge/ThermalAssistantWindow.hpp"
#include "Math/fixed.hpp"
#include "MainWindow.hpp"
#include "Compiler.h"

static WndForm *wf = NULL;
static ThermalAssistantWindow *wta;

/**
 * This event handler is called when the "Close" button is pressed
 */
static void
OnCloseClicked(gcc_unused WndButton &button)
{
  wf->SetModalResult(mrOK);
}

static Window *
OnCreateThermalAssistantControl(ContainerWindow &parent, int left, int top,
                            unsigned width, unsigned height,
                            const WindowStyle style)
{
  wta = new ThermalAssistantWindow(Layout::FastScale(10), false);
  wta->set(parent, left, top, width, height, style);
  return wta;
}

static void
Update()
{
  wta->Update(XCSoarInterface::Basic().Heading, XCSoarInterface::Calculated());
}

static void
OnTimerNotify(WindowControl * Sender)
{
  (void)Sender;
  Update();
}

static CallBackTableEntry CallBackTable[] = {
  DeclareCallBackEntry(OnCreateThermalAssistantControl),
  DeclareCallBackEntry(NULL)
};

void
dlgThermalAssistantShowModal()
{
  // Load dialog from XML
  wf = LoadDialog(CallBackTable, XCSoarInterface::main_window,
                      Layout::landscape ? _T("IDR_XML_THERMALASSISTANT_L") :
                                          _T("IDR_XML_THERMALASSISTANT"));
  if (!wf)
    return;

  wf->SetTimerNotify(OnTimerNotify);

  // Set button events
  ((WndButton *)wf->FindByName(_T("cmdClose")))->
      SetOnClickNotify(OnCloseClicked);

  Update();

  // Show the dialog
  wf->ShowModal();

  // After dialog closed -> delete it
  delete wf;
}
