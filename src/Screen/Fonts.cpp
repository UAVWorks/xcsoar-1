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

#include "Screen/Fonts.hpp"
#include "LogFile.hpp"
#include "UtilsFont.hpp"
#include "InfoBoxes/InfoBoxLayout.hpp"
#include "ButtonLabel.hpp"
#include "Profile/Profile.hpp"
#include "Screen/Layout.hpp"
#include "Screen/VirtualCanvas.hpp"
#include "Appearance.hpp"
#include "InfoBoxes/InfoBoxLayout.hpp"

#include <stdio.h>

/// values inside infoboxes  like numbers, etc.
Font Fonts::InfoBox;
Font Fonts::InfoBoxSmall;
/// Titles of infoboxes like Next, WP L/D etc.
Font Fonts::Title;
/// text names on the map
Font Fonts::Map;
/// menu buttons, waypoint selection, messages, etc.
Font Fonts::MapBold;
/// vario display, runway informations
Font Fonts::CDI;
/// Flarm Traffic draweing and stats, map labels in italic
Font Fonts::MapLabel;

// these are the non-custom parameters
LOGFONT LogInfoBox;
LOGFONT LogTitle;
LOGFONT LogMap;
LOGFONT LogInfoBoxSmall;
LOGFONT LogMapBold;
LOGFONT LogCDI;
LOGFONT LogMapLabel;

static void
InitialiseLogfont(LOGFONT* font, const TCHAR* facename, int height,
                  bool bold = false, bool italic = false,
                  bool variable_pitch = true)
{
  memset((char *)font, 0, sizeof(LOGFONT));

  _tcscpy(font->lfFaceName, facename);
  font->lfPitchAndFamily = (variable_pitch ? VARIABLE_PITCH : FIXED_PITCH)
                          | FF_DONTCARE;
  font->lfHeight = (long)height;
  font->lfWeight = (long)(bold ? FW_BOLD : FW_MEDIUM);
  font->lfItalic = italic;
  font->lfQuality = ANTIALIASED_QUALITY;
}

void
Fonts::LoadCustomFont(Font *theFont, const TCHAR FontRegKey[])
{
  LOGFONT logfont;
  memset((char *)&logfont, 0, sizeof(LOGFONT));
  if (Profile::GetFont(FontRegKey, &logfont))
    theFont->set(logfont);
}

static void
LoadAltairLogFonts()
{
  InitialiseLogfont(&LogInfoBox, _T("RasterGothicTwentyFourCond"), 24, true);
  InitialiseLogfont(&LogTitle, _T("RasterGothicNineCond"), 10);
  InitialiseLogfont(&LogCDI, _T("RasterGothicEighteenCond"), 19, true);
  InitialiseLogfont(&LogMapLabel, _T("RasterGothicTwelveCond"), 13);
  InitialiseLogfont(&LogMap, _T("RasterGothicFourteenCond"), 15);
  InitialiseLogfont(&LogMapBold, _T("RasterGothicFourteenCond"), 15, true);
  InitialiseLogfont(&LogInfoBoxSmall, _T("RasterGothicEighteenCond"), 19, true);
}

static void
SizeLogFont(LOGFONT &logfont, unsigned width, const TCHAR* str)
{
  // JMW algorithm to auto-size info window font.
  // this is still required in case title font property doesn't exist.
  VirtualCanvas canvas(1, 1);
  SIZE tsize;
  do {
    --logfont.lfHeight;

    Font font;
    if (!font.set(logfont))
      break;

    canvas.select(font);
    tsize = canvas.text_size(str);
  } while ((unsigned)tsize.cx > width);

  ++logfont.lfHeight;
}

static void
InitialiseLogFonts()
{
  if (is_altair()) {
    LoadAltairLogFonts();
    return;
  }

#ifdef ENABLE_SDL
  int FontHeight = Layout::SmallScale(20);
#else
  int FontHeight = Layout::SmallScale(35);
#endif

  // oversize first so can then scale down
  InitialiseLogfont(&LogInfoBox, Fonts::GetStandardFontFace(),
                    (int)(FontHeight * 1.4), true, false, true);

  LogInfoBox.lfCharSet = ANSI_CHARSET;

  InitialiseLogfont(&LogTitle, Fonts::GetStandardFontFace(),
                    FontHeight / 3, true);

  // new font for CDI Scale
  InitialiseLogfont(&LogCDI, Fonts::GetStandardFontFace(),
                    (int)(FontHeight * 0.6), false, false, false);

  // new font for map labels
  InitialiseLogfont(&LogMapLabel, Fonts::GetStandardFontFace(),
                    (int)(FontHeight * 0.39), false, true);

  // new font for map labels
  InitialiseLogfont(&LogMap, Fonts::GetStandardFontFace(),
                    (int)(FontHeight * 0.507));

  // Font for map bold text
  InitialiseLogfont(&LogMapBold, Fonts::GetStandardFontFace(),
                    (int)(FontHeight * 0.507), true);

  InitialiseLogfont(&LogInfoBoxSmall, Fonts::GetStandardFontFace(),
                    Layout::Scale(20));
}

void
Fonts::Initialize()
{
  InitialiseLogFonts();

  InfoBoxSmall.set(LogInfoBoxSmall);
  Title.set(LogTitle);
  CDI.set(LogCDI);
  MapLabel.set(LogMapLabel);
  Map.set(LogMap);
  MapBold.set(LogMapBold);
}

void
Fonts::SizeInfoboxFont()
{
  SizeLogFont(LogInfoBox, InfoBoxLayout::ControlWidth, _T("1234m"));
  InfoBox.set(LogInfoBox);
}

void
Fonts::LoadCustom()
{
  LoadCustomFont(&InfoBox, szProfileFontInfoWindowFont);
  LoadCustomFont(&InfoBoxSmall, szProfileFontTitleSmallWindowFont);
  LoadCustomFont(&Title, szProfileFontTitleWindowFont);
  LoadCustomFont(&CDI, szProfileFontCDIWindowFont);
  LoadCustomFont(&MapLabel, szProfileFontMapLabelFont);
  LoadCustomFont(&Map, szProfileFontMapWindowFont);
  LoadCustomFont(&MapBold, szProfileFontMapWindowBoldFont);
}

const TCHAR*
Fonts::GetStandardFontFace()
{
  if (is_altair())
    return _T("RasterGothicFourteenCond");

  return _T("Tahoma");
}
