/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2011 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

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

#include "InfoBoxes/InfoBoxManager.hpp"
#include "InfoBoxes/InfoBoxWindow.hpp"
#include "InfoBoxes/InfoBoxLayout.hpp"
#include "Look/InfoBoxLook.hpp"
#include "InfoBoxes/Content/Factory.hpp"
#include "InfoBoxes/Content/Base.hpp"
#include "Profile/InfoBoxConfig.hpp"
#include "InputEvents.hpp"
#include "Screen/Blank.hpp"
#include "Screen/Layout.hpp"
#include "Screen/Fonts.hpp"
#include "Screen/Graphics.hpp"
#include "Hardware/Battery.hpp"
#include "MainWindow.hpp"
#include "Language/Language.hpp"
#include "DataField/Enum.hpp"
#include "DataField/ComboList.hpp"
#include "Dialogs/Dialogs.h"
#include "Dialogs/ComboPicker.hpp"
#include "Profile/InfoBoxConfig.hpp"
#include "Interface.hpp"
#include "UIState.hpp"

#include <assert.h>
#include <stdio.h>

#include <algorithm>

namespace InfoBoxManager
{
  InfoBoxLayout::Layout layout;

  /** the window for displaying infoboxes full-screen */
  InfoBoxFullWindow full_window;

  /**
   * Is this the initial DisplayInfoBox() call?  If yes, then all
   * content objects need to be created.
   */
  static bool first;

  InfoBoxFactory::t_InfoBox GetCurrentType(unsigned box);

  void DisplayInfoBox();
  void InfoBoxDrawIfDirty();
  int GetFocused();

  int GetInfoBoxBorder(unsigned i);
}

static bool InfoBoxesDirty = false;
static bool InfoBoxesHidden = false;

InfoBoxWindow *InfoBoxes[InfoBoxSettings::Panel::MAX_CONTENTS];

void
InfoBoxFullWindow::on_paint(Canvas &canvas)
{
  canvas.clear_white();

  for (unsigned i = 0; i < InfoBoxManager::layout.count; i++) {
    // JMW TODO: make these calculated once only.
    int x, y;
    int rx, ry;
    int rw;
    int rh;
    double fw, fh;

    if (Layout::landscape) {
      rw = 84;
      rh = 68;
    } else {
      rw = 120;
      rh = 80;
    }

    fw = rw / (double)InfoBoxManager::layout.control_width;
    fh = rh / (double)InfoBoxManager::layout.control_height;

    double f = std::min(fw, fh);
    rw = (int)(f * InfoBoxManager::layout.control_width);
    rh = (int)(f * InfoBoxManager::layout.control_height);

    if (Layout::landscape) {
      rx = i % 3;
      ry = i / 3;

      x = (rw + 4) * rx;
      y = (rh + 3) * ry;
    } else {
      rx = i % 2;
      ry = i / 4;

      x = (rw) * rx;
      y = (rh) * ry;
    }

    assert(InfoBoxes[i] != NULL);
    InfoBoxes[i]->PaintInto(canvas, Layout::Scale(x), Layout::Scale(y),
                            Layout::Scale(rw), Layout::Scale(rh));
  }
}

// TODO locking
void
InfoBoxManager::Hide()
{
  InfoBoxesHidden = true;

  for (unsigned i = 0; i < layout.count; i++)
    InfoBoxes[i]->fast_hide();

  full_window.hide();
}

void
InfoBoxManager::Show()
{
  InfoBoxesHidden = false;

  for (unsigned i = 0; i < layout.count; i++)
    InfoBoxes[i]->show();
}

int
InfoBoxManager::GetFocused()
{
  for (unsigned i = 0; i < layout.count; i++)
    if (InfoBoxes[i]->has_focus())
      return i;

  return -1;
}

void
InfoBoxManager::Event_Select(int i)
{
  int InfoFocus = GetFocused();

  if (InfoFocus < 0) {
    InfoFocus = (i >= 0 ? 0 : layout.count - 1);
  } else {
    InfoFocus += i;

    if (InfoFocus < 0 || (unsigned)InfoFocus >= layout.count)
      InfoFocus = -1;
  }

  if (InfoFocus >= 0)
    XCSoarInterface::main_window.SetDefaultFocus();
  else
    InfoBoxes[i]->set_focus();
}

unsigned
InfoBoxManager::GetCurrentPanel()
{
  const UIState &ui_state = CommonInterface::GetUIState();

  if (ui_state.auxiliary_enabled) {
    unsigned panel = ui_state.auxiliary_index;
    if (panel >= InfoBoxSettings::MAX_PANELS)
      panel = PANEL_AUXILIARY;
    return panel;
  }
  else if (XCSoarInterface::main_window.GetDisplayMode() == DM_CIRCLING)
    return PANEL_CIRCLING;
  else if (XCSoarInterface::main_window.GetDisplayMode() == DM_FINAL_GLIDE)
    return PANEL_FINAL_GLIDE;
  else
    return PANEL_CRUISE;
}

const TCHAR*
InfoBoxManager::GetPanelName(unsigned panelIdx)
{
  const InfoBoxSettings &infoBoxManagerConfig =
    CommonInterface::GetUISettings().info_boxes;

  return gettext(infoBoxManagerConfig.panels[panelIdx].name);
}

const TCHAR*
InfoBoxManager::GetCurrentPanelName()
{
  return GetPanelName(GetCurrentPanel());
}

InfoBoxFactory::t_InfoBox
InfoBoxManager::GetType(unsigned box, unsigned panelIdx)
{
  assert(box < InfoBoxSettings::Panel::MAX_CONTENTS);
  assert(panelIdx < InfoBoxSettings::MAX_PANELS);

  const InfoBoxSettings &infoBoxManagerConfig =
    CommonInterface::GetUISettings().info_boxes;

  return infoBoxManagerConfig.panels[panelIdx].contents[box];
}

InfoBoxFactory::t_InfoBox
InfoBoxManager::GetCurrentType(unsigned box)
{
  InfoBoxFactory::t_InfoBox retval = GetType(box, GetCurrentPanel());
  return min(InfoBoxFactory::MAX_TYPE_VAL, retval);
}

const TCHAR*
InfoBoxManager::GetTitle(unsigned box)
{
  if (InfoBoxes[box] != NULL)
    return InfoBoxes[box]->GetTitle();
  else
    return NULL;
}

bool
InfoBoxManager::IsEmpty(unsigned panelIdx)
{
  const InfoBoxSettings &infoBoxManagerConfig =
    CommonInterface::GetUISettings().info_boxes;

  return infoBoxManagerConfig.panels[panelIdx].IsEmpty();
}

void
InfoBoxManager::Event_Change(int i)
{
  InfoBoxFactory::t_InfoBox j = InfoBoxFactory::MIN_TYPE_VAL;
  InfoBoxFactory::t_InfoBox k;

  int InfoFocus = GetFocused();
  if (InfoFocus < 0)
    return;

  InfoBoxSettings &settings = CommonInterface::SetUISettings().info_boxes;
  const unsigned panel_index = GetCurrentPanel();
  InfoBoxSettings::Panel &panel = settings.panels[panel_index];

  k = panel.contents[InfoFocus];
  if (i > 0)
    j = (InfoBoxFactory::t_InfoBox)InfoBoxFactory::GetNext(k);
  else if (i < 0)
    j = (InfoBoxFactory::t_InfoBox)InfoBoxFactory::GetPrevious(k);

  // TODO code: if i==0, go to default or reset

  if (j == k)
    return;

  panel.contents[InfoFocus] = j;

  InfoBoxes[InfoFocus]->UpdateContent();
  Paint();
}

void
InfoBoxManager::DisplayInfoBox()
{
  static int DisplayTypeLast[InfoBoxSettings::Panel::MAX_CONTENTS];

  // JMW note: this is updated every GPS time step

  for (unsigned i = 0; i < layout.count; i++) {
    // All calculations are made in a separate thread. Slow calculations
    // should apply to the function DoCalculationsSlow()
    // Do not put calculations here!

    InfoBoxFactory::t_InfoBox DisplayType = GetCurrentType(i);

    bool needupdate = ((DisplayType != DisplayTypeLast[i]) || first);

    if (needupdate) {
      InfoBoxes[i]->SetTitle(gettext(InfoBoxFactory::GetCaption(DisplayType)));
      InfoBoxes[i]->SetContentProvider(InfoBoxFactory::Create(DisplayType));
      InfoBoxes[i]->SetID(i);
      DisplayTypeLast[i] = DisplayType;
    }

    InfoBoxes[i]->UpdateContent();
  }

  Paint();

  first = false;
}

void
InfoBoxManager::ProcessKey(InfoBoxContent::InfoBoxKeyCodes keycode)
{
  int focus = GetFocused();
  if (focus < 0)
    return;

  if (InfoBoxes[focus] != NULL)
    InfoBoxes[focus]->HandleKey(keycode);

  InputEvents::HideMenu();

  SetDirty();

  ResetDisplayTimeOut();
}

InfoBoxContent::DialogContent*
InfoBoxManager::GetDialogContent(const int id)
{
  if (id < 0)
    return NULL;

  if (InfoBoxes[id] != NULL)
    return InfoBoxes[id]->GetDialogContent();

  return NULL;
}

void
InfoBoxManager::ProcessQuickAccess(const int id, const TCHAR *Value)
{
  if (id < 0)
    return;

  // do approciate action
  if (InfoBoxes[id] != NULL)
    InfoBoxes[id]->HandleQuickAccess(Value);

  SetDirty();

  ResetDisplayTimeOut();
}

bool
InfoBoxManager::HasFocus()
{
  return GetFocused() >= 0;
}

void
InfoBoxManager::InfoBoxDrawIfDirty()
{
  // No need to redraw map or infoboxes if screen is blanked.
  // This should save lots of battery power due to CPU usage
  // of drawing the screen

  if (InfoBoxesDirty && !InfoBoxesHidden &&
      !CommonInterface::GetUIState().screen_blanked) {
    DisplayInfoBox();
    InfoBoxesDirty = false;
  }
}

void
InfoBoxManager::SetDirty()
{
  InfoBoxesDirty = true;
}

void
InfoBoxManager::ProcessTimer()
{
  InfoBoxDrawIfDirty();
}

void
InfoBoxManager::Paint()
{
  if (!InfoBoxLayout::fullscreen) {
    full_window.hide();
  } else {
    full_window.invalidate();
    full_window.show();
  }
}

int
InfoBoxManager::GetInfoBoxBorder(unsigned i)
{
  const InfoBoxSettings &settings =
    CommonInterface::GetUISettings().info_boxes;

  if (settings.border_style == apIbTab)
    return 0;

  unsigned border = 0;

  switch (InfoBoxLayout::InfoBoxGeometry) {
  case InfoBoxLayout::ibTop4Bottom4:
    if (i < 4)
      border |= BORDERBOTTOM;
    else
      border |= BORDERTOP;

    if (i != 3 && i != 7)
      border |= BORDERRIGHT;
    break;

  case InfoBoxLayout::ibBottom8:
  case InfoBoxLayout::ibBottom8Vario:
    border |= BORDERTOP;

    if (i != 3 && i != 7)
      border |= BORDERRIGHT;
    break;

  case InfoBoxLayout::ibBottom12:
    border |= BORDERTOP;

    if (i != 5 && i != 11)
      border |= BORDERRIGHT;
    break;

  case InfoBoxLayout::ibTop8:
    border |= BORDERBOTTOM;

    if (i != 3 && i != 7)
      border |= BORDERRIGHT;
    break;

  case InfoBoxLayout::ibTop12:
    border |= BORDERBOTTOM;

    if (i != 5 && i != 11)
      border |= BORDERRIGHT;
    break;

  case InfoBoxLayout::ibLeft4Right4:
    if (i != 3 && i != 7)
      border |= BORDERBOTTOM;

    if (i < 4)
      border |= BORDERRIGHT;
    else
      border |= BORDERLEFT;
    break;

  case InfoBoxLayout::ibGNav2:
    if ((i != 0) && (i != 6))
      border |= BORDERTOP;
    if (i < 6)
      border |= BORDERRIGHT;
    else
      border |= BORDERLEFT;
    break;

  case InfoBoxLayout::ibLeft8:
    if (i != 3 && i != 7)
      border |= BORDERBOTTOM;

    border |= BORDERRIGHT;
    break;

  case InfoBoxLayout::ibRight8:
    if (i != 3 && i != 7)
      border |= BORDERBOTTOM;

    border |= BORDERLEFT;
    break;

  case InfoBoxLayout::ibGNav:
    if (i != 0)
      border |= BORDERTOP;
    if (i < 6)
      border |= BORDERLEFT|BORDERRIGHT;
    break;

  case InfoBoxLayout::ibSquare:
    break;

  case InfoBoxLayout::ibRight12:
    if (i % 6 != 0)
      border |= BORDERTOP;
    border |= BORDERLEFT;
    break;

  case InfoBoxLayout::ibRight24:
    if (i % 8 != 0)
      border |= BORDERTOP;
    border |= BORDERLEFT;
    break;
  }

  return border;
}

void
InfoBoxManager::Create(PixelRect rc, const InfoBoxLayout::Layout &_layout,
                       const InfoBoxLook &look)
{
  const InfoBoxSettings &settings =
    CommonInterface::GetUISettings().info_boxes;

  first = true;
  layout = _layout;

  WindowStyle style;
  style.hide();
  full_window.set(XCSoarInterface::main_window, rc.left, rc.top,
                  rc.right - rc.left, rc.bottom - rc.top, style);

  // create infobox windows
  for (unsigned i = layout.count; i-- > 0;) {
    const PixelRect &rc = layout.positions[i];
    int Border = GetInfoBoxBorder(i);

    InfoBoxes[i] = new InfoBoxWindow(XCSoarInterface::main_window,
                                     rc.left, rc.top,
                                     rc.right - rc.left, rc.bottom - rc.top,
                                     Border, settings, look);
  }

  SetDirty();
}

void
InfoBoxManager::Destroy()
{
  for (unsigned i = 0; i < layout.count; i++) {
    delete InfoBoxes[i];
    InfoBoxes[i] = NULL;
  }

  full_window.reset();
}

static const ComboList *info_box_combo_list;

static void
OnInfoBoxHelp(unsigned item)
{
  int type = (*info_box_combo_list)[item].DataFieldIndex;

  TCHAR caption[100];
  _stprintf(caption, _T("%s: %s"), _("InfoBox"), gettext(InfoBoxFactory::GetName(type)));

  const TCHAR* text = InfoBoxFactory::GetDescription(type);
  if (text)
    dlgHelpShowModal(XCSoarInterface::main_window, caption, gettext(text));
  else
    dlgHelpShowModal(XCSoarInterface::main_window, caption,
                     _("No help available on this item"));
}

void
InfoBoxManager::ShowDlgInfoBox(const int id)
{
  if (GetDialogContent(id))
    dlgInfoBoxAccessShowModal(XCSoarInterface::main_window, id);
  else SetupFocused(id);
}

void
InfoBoxManager::SetupFocused(const int id)
{
  int i;

  if (id < 0) i = GetFocused();
  else i = id;

  if (i < 0)
    return;

  InfoBoxSettings &settings = CommonInterface::SetUISettings().info_boxes;
  const unsigned panel_index = GetCurrentPanel();
  InfoBoxSettings::Panel &panel = settings.panels[panel_index];

  const InfoBoxFactory::t_InfoBox old_type = panel.contents[i];

  ComboList list;
  for (unsigned i = 0; i < InfoBoxFactory::NUM_TYPES; i++)
    list.Append(i, gettext(InfoBoxFactory::GetName(i)));

  list.Sort();
  list.ComboPopupItemSavedIndex = list.LookUp(old_type);

  /* let the user select */

  TCHAR caption[20];
  _stprintf(caption, _T("%s: %d"), _("InfoBox"), i + 1);
  info_box_combo_list = &list;
  int result = ComboPicker(XCSoarInterface::main_window, caption, list,
                           OnInfoBoxHelp);
  if (result < 0)
    return;

  /* was there a modification? */

  InfoBoxFactory::t_InfoBox new_type = (InfoBoxFactory::t_InfoBox)list[result].DataFieldIndex;
  if (new_type == old_type)
    return;

  /* yes: apply and save it */

  panel.contents[i] = new_type;
  DisplayInfoBox();

  Profile::Save(panel, panel_index);
}
