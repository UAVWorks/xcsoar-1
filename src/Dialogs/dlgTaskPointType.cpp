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

#include "Task/Factory/AbstractTaskFactory.hpp"
#include "Dialogs/dlgTaskHelpers.hpp"

#include <assert.h>
#include <stdio.h>

static SingleWindow *parent_window;
static WndForm *wf = NULL;
static WndListFrame* wPointTypes = NULL;
static bool task_modified = false;
static OrderedTask* ordered_task = NULL;
static OrderedTaskPoint* point = NULL;
static unsigned active_index = 0;
static const Waypoint* way_point = NULL;

static AbstractTaskFactory::LegalPointVector point_types;

static void OnCloseClicked(WindowControl * Sender)
{
  (void)Sender;
  wf->SetModalResult(mrOK);
}

static AbstractTaskFactory::LegalPointType_t
get_point_type() 
{
  return ordered_task->get_factory().getType(point);
}


static AbstractTaskFactory::LegalPointType_t
get_cursor_type() 
{
  return point_types[wPointTypes->GetCursorIndex()];
}

static void
RefreshView()
{
  WndFrame* wSummary = (WndFrame *)wf->FindByName(_T("frmSummary"));
  if (wSummary) {
    const TCHAR* text = OrderedTaskPointDescription(get_cursor_type());
    wSummary->SetCaption(text);
  }
}


static void
OnPointPaintListItem(Canvas &canvas, const RECT rc, unsigned DrawListIndex)
{
  assert(DrawListIndex < point_types.size());

  TCHAR sTmp[120];

  const TCHAR* text = OrderedTaskPointName(point_types[DrawListIndex]);

  if (point && (point_types[DrawListIndex] == get_point_type()))
    _stprintf(sTmp, _T("*%s"), text);
  else
    _stprintf(sTmp, _T(" %s"), text);

  canvas.text(rc.left + Layout::FastScale(2),
              rc.top + Layout::FastScale(2), sTmp);
}

static bool
SetPointType(AbstractTaskFactory::LegalPointType_t type)
{
  bool apply = false;

  if (!point) {
    apply = true;
    // empty point, don't ask confirmation
  } else {
    if (type == get_point_type())
      // no change
      return true;

    if (MessageBoxX(_("Change point type?"), _("Task Point"),
                    MB_YESNO | MB_ICONQUESTION) == IDYES)
      apply = true;
  }

  if (apply) {
    AbstractTaskFactory &factory = ordered_task->get_factory();

    if (point) {
      point = factory.createPoint(type, point->get_waypoint());
      if (factory.replace(point, active_index, true))
        task_modified = true;
    } else {
      if (factory.validFinishType(type) &&
          ordered_task->get_ordered_task_behaviour().is_closed)
        way_point = &(ordered_task->get_tp(0)->get_waypoint());
      else
        way_point =
            dlgWayPointSelect(*parent_window,
                              ordered_task->task_size() > 0 ?
                              ordered_task->get_tp(ordered_task->
                                  task_size() - 1)->get_location() :
                              XCSoarInterface::Basic().Location);
      if (!way_point)
        return false;

      point = factory.createPoint(type, *way_point);
      if (point && factory.append(point, true))
        task_modified = true;
    }
    return true;
  }
  return false;
}

static void
OnSelect()
{
  if (wPointTypes->GetCursorIndex() >= point_types.size())
    return;

  if (SetPointType(get_cursor_type()))
    wf->SetModalResult(mrOK);
  else
    wf->SetModalResult(mrCancel);
}

static void 
OnSelectClicked(WindowControl * Sender)
{
  OnSelect();
}

static void
OnPointListEnter(unsigned ItemIndex)
{
  OnSelect();
}

static void
OnPointCursorCallback(unsigned i)
{
  RefreshView();
}

static CallBackTableEntry CallBackTable[] = {
  DeclareCallBackEntry(OnCloseClicked),
  DeclareCallBackEntry(OnSelectClicked),
  DeclareCallBackEntry(NULL)
};

bool
dlgTaskPointNew(SingleWindow &parent, OrderedTask** task, const unsigned index)
{
  return dlgTaskPointType(parent, task, index);
}

bool
dlgTaskPointType(SingleWindow &parent, OrderedTask** task, const unsigned index)
{
  ordered_task = *task;
  parent_window = &parent;
  task_modified = false;
  active_index = index;

  point = ordered_task->get_tp(active_index);
  if (point)
    way_point = &point->get_waypoint();
  else
    way_point = NULL;

  if (Layout::landscape)
    wf = LoadDialog(CallBackTable, parent, _T("IDR_XML_TASKPOINTTYPE_L"));
  else
    wf = LoadDialog(CallBackTable, parent, _T("IDR_XML_TASKPOINTTYPE"));

  if (!wf)
    return false;

  assert(wf != NULL);

  wPointTypes = (WndListFrame*)wf->FindByName(_T("frmPointTypes"));
  assert(wPointTypes != NULL);

  point_types = ordered_task->get_factory().getValidTypes(index);
  if (point_types.empty()) {
    assert(1);
    return false;
  }

  wPointTypes->SetActivateCallback(OnPointListEnter);
  wPointTypes->SetPaintItemCallback(OnPointPaintListItem);
  wPointTypes->SetCursorCallback(OnPointCursorCallback);
  wPointTypes->SetLength(point_types.size());

  if (point)
    for (unsigned i=0; i<point_types.size(); i++)
      if (point_types[i] == get_point_type())
        wPointTypes->SetCursorIndex(i); 

  RefreshView();

  if (point_types.size()==1)
    SetPointType(point_types[0]);
  else
    wf->ShowModal();

  delete wf;
  wf = NULL;

  return task_modified;
}
