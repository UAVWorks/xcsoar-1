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

#ifndef XCSOAR_IO_FILE_SOURCE_HPP
#define XCSOAR_IO_FILE_SOURCE_HPP

#include "BufferedSource.hpp"

#ifdef HAVE_POSIX

class PosixFileSource : public BufferedSource<char> {
private:
  int fd;

public:
  PosixFileSource(const char *path);
  virtual ~PosixFileSource();

  bool error() const {
    return fd < 0;
  }

public:
  virtual long size() const;

protected:
  virtual unsigned read(char *p, unsigned n);
};

#endif /* HAVE_POSIX */

#ifdef WIN32

#include <windows.h>
#include <tchar.h>

class WindowsFileSource : public BufferedSource<char> {
private:
  HANDLE handle;

public:
  WindowsFileSource(const char *path);
  virtual ~WindowsFileSource();

#ifdef _UNICODE
  WindowsFileSource(const TCHAR *path);
#endif

  bool error() const {
    return handle == INVALID_HANDLE_VALUE;
  }

public:
  virtual long size() const;

protected:
  virtual unsigned read(char *p, unsigned n);
};

#endif /* WIN32 */

#if defined(WIN32) && !defined(__CYGWIN__)
class FileSource : public WindowsFileSource {
public:
  FileSource(const char *path):WindowsFileSource(path) {}

#ifdef _UNICODE
  FileSource(const TCHAR *path):WindowsFileSource(path) {}
#endif
};
#else
class FileSource : public PosixFileSource {
public:
  FileSource(const char *path):PosixFileSource(path) {}
};
#endif

#endif
