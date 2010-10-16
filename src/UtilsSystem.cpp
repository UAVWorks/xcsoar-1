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

#include "UtilsSystem.hpp"
#include "Interface.hpp"
#include "Asset.hpp"
#include "LocalPath.hpp"
#include "LogFile.hpp"
#include "Asset.hpp"
#include "OS/FileUtil.hpp"

#include <tchar.h>

#ifdef HAVE_POSIX
#ifndef ANDROID
#include <sys/statvfs.h>
#endif
#include <sys/stat.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

static long
CheckFreeRam()
{
#ifdef WIN32
  MEMORYSTATUS memInfo;
  // Program memory
  memInfo.dwLength = sizeof(memInfo);
  GlobalMemoryStatus(&memInfo);

  //	   memInfo.dwTotalPhys,
  //	   memInfo.dwAvailPhys,
  //	   memInfo.dwTotalPhys- memInfo.dwAvailPhys);

  return memInfo.dwAvailPhys;
#else /* !WIN32 */
  return 64 * 1024 * 1024; // XXX
#endif /* !WIN32 */
}

// This is necessary to be called periodically to get rid of
// memory defragmentation, since on pocket pc platforms there is no
// automatic defragmentation.
void MyCompactHeaps() {
#ifdef WIN32
#if !defined(_WIN32_WCE) || (defined(GNAV) && !defined(__GNUC__))
  HeapCompact(GetProcessHeap(), 0);
#else
  typedef DWORD (_stdcall *CompactAllHeapsFn) (void);
  static CompactAllHeapsFn CompactAllHeaps = NULL;
  static bool init = false;
  if (!init) {
    // get the pointer to the function
    CompactAllHeaps = (CompactAllHeapsFn)GetProcAddress(
        LoadLibrary(_T("coredll.dll")), _T("CompactAllHeaps"));
    init = true;
  }
  if (CompactAllHeaps)
    CompactAllHeaps();
#endif
#endif /* WIN32 */
}

/**
 * Calculates the free disk space for the given path
 * @param path The path defining the "drive" to look on
 * @return Number of KiB free on the destination drive
 */
unsigned long FindFreeSpace(const TCHAR *path) {
#ifdef HAVE_POSIX
#ifdef ANDROID
  return 64 * 1024 * 1024;
#else
  struct statvfs s;
  if (statvfs(path, &s) < 0)
    return 0;
  return s.f_bsize * s.f_bavail;
#endif
#else /* !HAVE_POSIX */
  ULARGE_INTEGER FreeBytesAvailableToCaller;
  ULARGE_INTEGER TotalNumberOfBytes;
  ULARGE_INTEGER TotalNumberOfFreeBytes;
  if (GetDiskFreeSpaceEx(path,
                         &FreeBytesAvailableToCaller,
                         &TotalNumberOfBytes,
                         &TotalNumberOfFreeBytes)) {
    return FreeBytesAvailableToCaller.LowPart / 1024;
  } else
    return 0;
#endif /* !HAVE_POSIX */
}

/**
 * Creates a new directory in the home directory, if it doesn't exist yet
 * @param filename Name of the new directory
 */
void CreateDirectoryIfAbsent(const TCHAR *filename) {
  TCHAR fullname[MAX_PATH];

  LocalPath(fullname, filename);
  if (Directory::Exists(fullname))
    return;

#ifdef HAVE_POSIX
  mkdir(filename, 0777);
#else /* !HAVE_POSIX */
  CreateDirectory(fullname, NULL);
#endif /* !HAVE_POSIX */
}

void
StartupLogFreeRamAndStorage()
{
  int freeram = CheckFreeRam() / 1024;
  int freestorage = FindFreeSpace(GetPrimaryDataPath());
  LogStartUp(_T("Free ram %d; free storage %d"), freeram, freestorage);
}

unsigned
TranscodeKey(unsigned wParam)
{
  // VENTA-ADDON HARDWARE KEYS TRANSCODING

  if (GlobalModelType == MODELTYPE_PNA_HP31X) {
    if (wParam == 0x7b)
      wParam = 0x1b;
  } else if (GlobalModelType == MODELTYPE_PNA_PN6000) {
    switch(wParam) {
    case 0x79:					// Upper Silver key short press
      wParam = 0xc1;	// F10 -> APP1
      break;
    case 0x7b:					// Lower Silver key short press
      wParam = 0xc2;	// F12 -> APP2
      break;
    case 0x72:					// Back key plus
      wParam = 0xc3;	// F3  -> APP3
      break;
    case 0x71:					// Back key minus
      wParam = 0xc4;	// F2  -> APP4
      break;
    case 0x7a:					// Upper silver key LONG press
      wParam = 0x70;	// F11 -> F1
      break;
    case 0x7c:					// Lower silver key LONG press
      wParam = 0x71;	// F13 -> F2
      break;
    }
  } else if (GlobalModelType == MODELTYPE_PNA_NOKIA_500) {
    switch(wParam) {
    case 0xc1:
      wParam = 0x0d;	// middle key = enter
      break;
    case 0xc5:
      wParam = 0x26;	// + key = pg Up
      break;
    case 0xc6:
      wParam = 0x28;	// - key = pg Down
      break;
    }
  } else if (GlobalModelType == MODELTYPE_PNA_MEDION_P5) {
    switch(wParam) {
    case 0x79:
      wParam = 0x0d;	// middle key = enter
      break;
    case 0x75:
      wParam = 0x26;	// + key = pg Up
      break;
    case 0x76:
      wParam = 0x28;	// - key = pg Down
      break;
    }
  }

  return wParam;
}

/**
 * Returns the screen dimension rect to be used
 * @return The screen dimension rect to be used
 */
RECT SystemWindowSize(void) {
  RECT WindowSize;

#if defined(WIN32) && !defined(_WIN32_WCE)
  WindowSize.right = SCREENWIDTH + 2 * GetSystemMetrics(SM_CXFIXEDFRAME);
  WindowSize.left = (GetSystemMetrics(SM_CXSCREEN) - WindowSize.right) / 2;
  WindowSize.bottom = SCREENHEIGHT + 2 * GetSystemMetrics(SM_CYFIXEDFRAME) +
                      GetSystemMetrics(SM_CYCAPTION);
  WindowSize.top = (GetSystemMetrics(SM_CYSCREEN) - WindowSize.bottom) / 2;
#else
  WindowSize.left = 0;
  WindowSize.top = 0;

  #ifdef WIN32
  WindowSize.right = GetSystemMetrics(SM_CXSCREEN);
  WindowSize.bottom = GetSystemMetrics(SM_CYSCREEN);
  #else /* !WIN32 */
  /// @todo implement this properly for SDL/UNIX
  WindowSize.right = 640;
  WindowSize.bottom = 480;
  #endif /* !WIN32 */

#endif

  return WindowSize;
}
