/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2010 The XCSoar Project
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

#include "DataField/Enum.hpp"
#include "DataField/ComboList.hpp"

#include <stdlib.h>

#ifndef WIN32
#define _cdecl
#endif

DataFieldEnum::Entry::~Entry()
{
  free(mText);
}

int
DataFieldEnum::GetAsInteger() const
{
  if (entries.empty()) {
    assert(mValue == 0);
    return 0;
  } else {
    assert(mValue < entries.size());
    return entries[mValue].id;
  }
}

void
DataFieldEnum::replaceEnumText(unsigned int i, const TCHAR *Text)
{
  if (i <= entries.size()) {
    free(entries[i].mText);
    entries[i].mText = _tcsdup(Text);
 }
}

bool
DataFieldEnum::addEnumText(const TCHAR *Text, unsigned id)
{
  if (entries.full())
    return false;

  Entry &entry = entries.append();
  entry.mText = _tcsdup(Text);
  entry.id = id;
  return true;
}

unsigned
DataFieldEnum::addEnumText(const TCHAR *Text)
{
  if (entries.full())
    return 0;

  unsigned i = entries.size();
  Entry &entry = entries.append();
  entry.mText = _tcsdup(Text);
  entry.id = i;
  return i;
}

void
DataFieldEnum::addEnumTexts(const TCHAR *const*list)
{
  while (*list != NULL)
    addEnumText(*list++);
}

const TCHAR *
DataFieldEnum::GetAsString() const
{
  if (entries.empty()) {
    assert(mValue == 0);
    return NULL;
  } else {
    assert(mValue < entries.size());
    return entries[mValue].mText;
  }
}

void
DataFieldEnum::Set(int Value)
{
  int i = Find(Value);
  if (i >= 0)
    SetIndex(i);
}

void
DataFieldEnum::SetAsInteger(int Value)
{
  Set(Value);
}

void
DataFieldEnum::SetAsString(const TCHAR *Value)
{
  int i = Find(Value);
  if (i >= 0)
    SetIndex(i);
}

void
DataFieldEnum::Inc(void)
{
  if (entries.empty()) {
    assert(mValue == 0);
    return;
  }

  assert(mValue < entries.size());

  if (mValue < entries.size() - 1) {
    mValue++;
    if (!GetDetachGUI())
      (mOnDataAccess)(this, daChange);
  }
}

void
DataFieldEnum::Dec(void)
{
  if (entries.empty()) {
    assert(mValue == 0);
    return;
  }

  assert(mValue < entries.size());

  if (mValue > 0) {
    mValue--;
    if (!GetDetachGUI())
      (mOnDataAccess)(this, daChange);
  }
}

static int _cdecl
DataFieldEnumCompare(const void *elem1, const void *elem2)
{
  const DataFieldEnum::Entry *entry1 = (const DataFieldEnum::Entry *)elem1;
  const DataFieldEnum::Entry *entry2 = (const DataFieldEnum::Entry *)elem2;

  return _tcscmp(entry1->mText, entry2->mText);
}

void
DataFieldEnum::Sort(int startindex)
{
  qsort(entries.begin() + startindex, entries.size() - startindex,
        sizeof(entries[0]),
        DataFieldEnumCompare);
}

ComboList *
DataFieldEnum::CreateComboList() const
{
  ComboList *combo_list = new ComboList();

  for (unsigned i = 0; i < entries.size(); i++)
    combo_list->Append(entries[i].id, entries[i].mText);

  combo_list->ComboPopupItemSavedIndex = mValue;
  return combo_list;
}

int
DataFieldEnum::Find(const TCHAR *text) const
{
  assert(text != NULL);

  for (unsigned int i = 0; i < entries.size(); i++)
    if (_tcscmp(text, entries[i].mText) == 0)
      return i;

  return -1;
}

int
DataFieldEnum::Find(unsigned id) const
{
  for (unsigned i = 0; i < entries.size(); i++)
    if (entries[i].id == id)
      return i;

  return -1;
}

void
DataFieldEnum::SetIndex(unsigned new_value)
{
  assert(new_value < entries.size());

  if (new_value == mValue)
    return;

  mValue = new_value;
  if (!GetDetachGUI())
    mOnDataAccess(this, daChange);
}
