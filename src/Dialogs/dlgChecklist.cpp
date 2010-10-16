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
#include "Screen/Layout.hpp"
#include "LocalPath.hpp"
#include "UtilsText.hpp"
#include "MainWindow.hpp"
#include "Defines.h"
#include "StringUtil.hpp"
#include "IO/DataFile.hpp"
#include "Compiler.h"

#include <assert.h>

#define XCSCHKLIST  "xcsoar-checklist.txt"

#define MAXTITLE 200
#define MAXDETAILS 5000

static int page = 0;
static WndForm *wf = NULL;
static WndListFrame *wDetails = NULL;

#define MAXLINES 100
#define MAXLISTS 20
static int LineOffsets[MAXLINES];
static unsigned nTextLines;
static int nLists = 0;
static TCHAR *ChecklistText[MAXTITLE];
static TCHAR *ChecklistTitle[MAXTITLE];

static void
NextPage(int Step)
{
  TCHAR buffer[80];

  page += Step;
  if (page >= nLists)
    page = 0;
  if (page < 0)
    page = nLists - 1;

  nTextLines = TextToLineOffsets(ChecklistText[page], LineOffsets, MAXLINES);

  _tcscpy(buffer, _("Checklist"));

  if (ChecklistTitle[page] &&
      !string_is_empty(ChecklistTitle[page]) &&
      _tcslen(ChecklistTitle[page]) < 60) {
    _tcscat(buffer, _T(": "));
    _tcscat(buffer, ChecklistTitle[page]);
  }
  wf->SetCaption(buffer);

  wDetails->SetLength(nTextLines);
  wDetails->invalidate();
}

static void
OnPaintDetailsListItem(Canvas &canvas, const RECT rc, unsigned i)
{
  assert(i < nTextLines);

  TCHAR* text = ChecklistText[page];
  if (text == NULL)
    return;

  int nstart = LineOffsets[i];
  int nlen;
  if (i < nTextLines - 1) {
    nlen = LineOffsets[i + 1] - LineOffsets[i];
    nlen--;
  } else {
    nlen = _tcslen(text + nstart) - 1;
  }

  if (nlen > 0)
    canvas.text(rc.left + Layout::FastScale(2), rc.top + Layout::FastScale(2),
                text + nstart, nlen);
}

static void
OnNextClicked(gcc_unused WndButton &button)
{
  NextPage(+1);
}

static void
OnPrevClicked(gcc_unused WndButton &button)
{
  NextPage(-1);
}

static void
OnCloseClicked(gcc_unused WndButton &button)
{
  wf->SetModalResult(mrOK);
}

static bool
FormKeyDown(WindowControl *Sender, unsigned key_code)
{
  (void)Sender;

  switch (key_code) {
    case VK_LEFT:
    case '6':
      ((WndButton *)wf->FindByName(_T("cmdPrev")))->set_focus();
      NextPage(-1);
    return true;

    case VK_RIGHT:
    case '7':
      ((WndButton *)wf->FindByName(_T("cmdNext")))->set_focus();
      NextPage(+1);
    return true;

  default:
    return false;
  }
}

static CallBackTableEntry CallBackTable[] = {
  DeclareCallBackEntry(OnNextClicked),
  DeclareCallBackEntry(OnPrevClicked),
  DeclareCallBackEntry(NULL)
};

static void
addChecklist(const TCHAR *name, const TCHAR *details)
{
  if (nLists >= MAXLISTS)
    return;

  ChecklistTitle[nLists] = _tcsdup(name);
  ChecklistText[nLists] = _tcsdup(details);
  nLists++;
}

static void
LoadChecklist(void)
{
  nLists = 0;

  free(ChecklistText[0]);
  ChecklistText[0] = NULL;

  free(ChecklistTitle[0]);
  ChecklistTitle[0] = NULL;

  TLineReader *reader = OpenDataTextFile(_T(XCSCHKLIST));
  if (reader == NULL)
    return;

  TCHAR Details[MAXDETAILS];
  TCHAR Name[100];
  BOOL inDetails = FALSE;
  int i;

  Details[0] = 0;
  Name[0] = 0;

  TCHAR *TempString;
  while ((TempString = reader->read()) != NULL) {
    // Look for start
    if (TempString[0] == '[') {
      if (inDetails) {
        addChecklist(Name, Details);
        Details[0] = 0;
        Name[0] = 0;
      }

      // extract name
      for (i = 1; i < MAXTITLE; i++) {
        if (TempString[i] == ']')
          break;

        Name[i - 1] = TempString[i];
      }
      Name[i - 1] = 0;

      inDetails = true;
    } else {
      // append text to details string
      _tcsncat(Details, TempString, MAXDETAILS - 2);
      _tcscat(Details, _T("\n"));
      // TODO code: check the string is not too long
    }
  }

  delete reader;

  if (inDetails) {
    addChecklist(Name, Details);
  }
}

void
dlgChecklistShowModal(void)
{
  static bool first = true;
  if (first) {
    LoadChecklist();
    first = false;
  }

  wf = LoadDialog(CallBackTable, XCSoarInterface::main_window,
                      Layout::landscape ?
                      _T("IDR_XML_CHECKLIST_L") : _T("IDR_XML_CHECKLIST"));
  if (!wf)
    return;

  nTextLines = 0;

  wf->SetKeyDownNotify(FormKeyDown);

  ((WndButton *)wf->FindByName(_T("cmdClose")))->SetOnClickNotify(OnCloseClicked);

  wDetails = (WndListFrame*)wf->FindByName(_T("frmDetails"));
  assert(wDetails != NULL);

  wDetails->SetPaintItemCallback(OnPaintDetailsListItem);

  page = 0;
  NextPage(0); // JMW just to turn proper pages on/off

  wf->ShowModal();

  delete wf;
}

