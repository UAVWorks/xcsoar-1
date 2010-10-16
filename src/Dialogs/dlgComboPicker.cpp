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
#include "Dialogs/ListPicker.hpp"
#include "InputEvents.h"
#include "DataField/Base.hpp"
#include "DataField/ComboList.hpp"
#include "Screen/Layout.hpp"

#include <assert.h>

static WndProperty *wComboPopupWndProperty;
static DataField *ComboPopupDataField;
static const ComboList *ComboListPopup;

static void
OnPaintComboPopupListItem(Canvas &canvas, const RECT rc, unsigned i)
{
  assert(i < (unsigned)ComboListPopup->size());

  canvas.text_clipped(rc.left + Layout::FastScale(2),
                      rc.top + Layout::FastScale(2), rc,
                      (*ComboListPopup)[i].StringValueFormatted);
}

int
ComboPicker(SingleWindow &parent, const TCHAR *caption,
            const ComboList &combo_list,
            ListHelpCallback_t help_callback)
{
  ComboListPopup = &combo_list;

  return ListPicker(parent, caption,
                    combo_list.size(),
                    combo_list.ComboPopupItemSavedIndex,
                    Layout::Scale(18),
                    OnPaintComboPopupListItem,
                    help_callback);
}

static void
OnHelpClicked(unsigned i)
{
  if (i < ComboListPopup->size()) {
    const ComboList::Item &item = (*ComboListPopup)[i];
    ComboPopupDataField->SetFromCombo(item.DataFieldIndex,
                                      item.StringValue);
  }

  wComboPopupWndProperty->OnHelp();
}

static int
ComboPicker(SingleWindow &parent, const WndProperty &control,
            const ComboList &combo_list)
{
  return ComboPicker(parent, control.GetCaption(), combo_list, OnHelpClicked);
}

int
dlgComboPicker(SingleWindow &parent, WndProperty *theProperty)
{
  static bool bInComboPicker = false;
  bool bInitialPage = true;
  // used to exit loop (optionally reruns combo with
  // lower/higher index of items for int/float
  bool bOpenCombo = true;

  // prevents multiple instances
  if (bInComboPicker)
    return 0;

  bInComboPicker = true;

  TCHAR sSavedInitialValue[100];
  int iSavedInitialDataIndex = -1;

  while (bOpenCombo) {
    assert(theProperty != NULL);
    wComboPopupWndProperty = theProperty;

    ComboPopupDataField = wComboPopupWndProperty->GetDataField();
    assert(ComboPopupDataField != NULL);

    ComboListPopup = ComboPopupDataField->CreateComboList();
    if (bInitialPage) { // save values for "Cancel" from first page only
      bInitialPage = false;
      iSavedInitialDataIndex =
        (*ComboListPopup)[ComboListPopup->ComboPopupItemSavedIndex]
        .DataFieldIndex;
      ComboPopupDataField->CopyString(sSavedInitialValue, false);
    }

    int idx = ComboPicker(parent, *theProperty, *ComboListPopup);

    bOpenCombo = false; //tell  combo to exit loop after close

    if (idx >= 0 && (unsigned)idx < ComboListPopup->size()) {
      const ComboList::Item *item = &(*ComboListPopup)[idx];

      // OK/Select
      if (item->DataFieldIndex == ComboList::Item::NEXT_PAGE) {
        // we're last in list and the want more past end of list so select last real list item and reopen
        ComboPopupDataField->SetDetachGUI(true);
        // we'll reopen, so don't call xcsoar data changed routine yet
        item = &(*ComboListPopup)[idx - 1];
        bOpenCombo = true; // reopen combo with new selected index at center
      } else if (item->DataFieldIndex == ComboList::Item::PREVIOUS_PAGE) {
        // same as above but lower items needed
        ComboPopupDataField->SetDetachGUI(true);
        item = &(*ComboListPopup)[idx + 1];
        bOpenCombo = true;
      }

      ComboPopupDataField->SetFromCombo(item->DataFieldIndex,
                                        item->StringValue);
    } else {
      // Cancel
      // if we've detached the GUI during the load, then there is nothing to do here
      assert(iSavedInitialDataIndex >= 0);
      if (iSavedInitialDataIndex >= 0)
        // use statics here - saved from first page if multiple were used
        ComboPopupDataField->SetFromCombo(iSavedInitialDataIndex,
            sSavedInitialValue);
    }

    delete ComboListPopup;

    wComboPopupWndProperty->RefreshDisplay();
  } // loop reopen combo if <<More>>  or <<Less>> picked

  bInComboPicker = false;
  return 1;
}
