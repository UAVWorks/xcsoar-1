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

#include "Screen/Canvas.hpp"
#include "Screen/Layout.hpp"
#include "Screen/Util.hpp"
#include "Compatibility/gdi.h"
#include "Asset.hpp" /* for needclipping */

#include <assert.h>
#include <string.h>
#include <stdlib.h> /* for abs() */

#ifdef ENABLE_SDL

#include <SDL/SDL_rotozoom.h>
#include <SDL/SDL_imageFilter.h>


void
Canvas::reset()
{
  if (surface != NULL) {
    SDL_FreeSurface(surface);
    surface = NULL;
  }
}

void
Canvas::move_to(int x, int y)
{
  cursor.x = x;
  cursor.y = y;
}

void
Canvas::line_to(int x, int y)
{
  line(cursor.x, cursor.y, x, y);
  move_to(x, y);
}

void
Canvas::segment(int x, int y, unsigned radius,
                Angle start, Angle end, bool horizon)
{
  // XXX horizon

  if (!brush.is_hollow())
    ::filledPieColor(surface, x, y, radius, 
                     (int)start.value_degrees() - 90,
                     (int)end.value_degrees() - 90,
                     brush.get_color().gfx_color());

  if (pen_over_brush())
    ::pieColor(surface, x, y, radius, 
               (int)start.value_degrees() - 90,
               (int)end.value_degrees() - 90,
               pen.get_color().gfx_color());
}

void
Canvas::draw_button(RECT rc, bool down)
{
  const Pen old_pen = pen;

  Brush gray(Color(192, 192, 192));
  fill_rectangle(rc, gray);

  Pen bright(1, Color(240, 240, 240));
  Pen dark(1, Color(128, 128, 128));

  select(down ? dark : bright);
  two_lines(rc.left, rc.bottom - 2, rc.left, rc.top,
            rc.right - 2, rc.top);
  two_lines(rc.left + 1, rc.bottom - 3, rc.left + 1, rc.top + 1,
            rc.right - 3, rc.top + 1);

  select(down ? bright : dark);
  two_lines(rc.left + 1, rc.bottom - 1, rc.right - 1, rc.bottom - 1,
            rc.right - 1, rc.top + 1);
  two_lines(rc.left + 2, rc.bottom - 2, rc.right - 2, rc.bottom - 2,
            rc.right - 2, rc.top + 2);

  pen = old_pen;
}

const SIZE
Canvas::text_size(const TCHAR *text, size_t length) const
{
  TCHAR *duplicated = _tcsdup(text);
  duplicated[length] = 0;

  const SIZE size = text_size(duplicated);
  free(duplicated);

  return size;
}

const SIZE
Canvas::text_size(const TCHAR *text) const
{
  SIZE size = { 0, 0 };

  if (font == NULL)
    return size;

  int ret, w, h;
#ifdef UNICODE
  ret = ::TTF_SizeUNICODE(font, (const Uint16 *)text, &w, &h);
#else
  ret = ::TTF_SizeText(font, text, &w, &h);
#endif
  if (ret == 0) {
    size.cx = w;
    size.cy = h;
  }

  return size;
}

void
Canvas::text(int x, int y, const TCHAR *text)
{
  SDL_Surface *s;

  if (font == NULL)
    return;

#ifdef UNICODE
  s = ::TTF_RenderUNICODE_Solid(font, (const Uint16 *)text, text_color);
#else
  s = ::TTF_RenderText_Solid(font, text, text_color);
#endif
  if (s == NULL)
    return;

  SDL_Rect dest = { x, y };
  // XXX non-opaque?
  ::SDL_BlitSurface(s, NULL, surface, &dest);
  ::SDL_FreeSurface(s);
}

void
Canvas::text_opaque(int x, int y, const RECT &rc, const TCHAR *_text)
{
  fill_rectangle(rc, background_color);
  text(x, y, _text);
}

void
Canvas::bottom_right_text(int x, int y, const TCHAR *_text)
{
  SIZE size = text_size(_text);
  text(x - size.cx, y - size.cy, _text);
}

void
Canvas::copy(int dest_x, int dest_y,
             unsigned dest_width, unsigned dest_height,
             const Canvas &src, int src_x, int src_y)
{
  assert(src.surface != NULL);

  SDL_Rect src_rect = { src_x, src_y, dest_width, dest_height };
  SDL_Rect dest_rect = { dest_x, dest_y };

  ::SDL_BlitSurface(src.surface, &src_rect, surface, &dest_rect);
}

void
Canvas::copy(const Canvas &src, int src_x, int src_y)
{
  copy(0, 0, src.surface->w, src.surface->h, src, src_x, src_y);
}

void
Canvas::copy(const Canvas &src)
{
  copy(src, 0, 0);
}

void
Canvas::copy_transparent_white(const Canvas &src)
{
  assert(src.surface != NULL);

  ::SDL_SetColorKey(src.surface, SDL_SRCCOLORKEY, src.map(Color::WHITE));
  copy(src);
  ::SDL_SetColorKey(src.surface, 0, 0);
}

void
Canvas::copy_transparent_black(const Canvas &src)
{
  assert(src.surface != NULL);

  ::SDL_SetColorKey(src.surface, SDL_SRCCOLORKEY, src.map(Color::BLACK));
  copy(src);
  ::SDL_SetColorKey(src.surface, 0, 0);
}

void
Canvas::stretch_transparent(const Canvas &src, Color key)
{
  assert(src.surface != NULL);

  ::SDL_SetColorKey(src.surface, SDL_SRCCOLORKEY, src.map(key));
  stretch(src);
  ::SDL_SetColorKey(src.surface, 0, 0);
}

void
Canvas::stretch(int dest_x, int dest_y,
                unsigned dest_width, unsigned dest_height,
                const Canvas &src,
                int src_x, int src_y,
                unsigned src_width, unsigned src_height)
{
  assert(src.surface != NULL);

  SDL_Surface *zoomed =
    ::zoomSurface(src.surface, (double)dest_width / (double)src_width,
                  (double)dest_height / (double)src_height,
                  SMOOTHING_OFF);

  if (zoomed == NULL)
    return;

  ::SDL_SetColorKey(zoomed, 0, 0);

  SDL_Rect src_rect = {
    (src_x * dest_width) / src_width,
    (src_y * dest_height) / src_height,
    dest_width, dest_height
  };
  SDL_Rect dest_rect = { dest_x, dest_y };

  ::SDL_BlitSurface(zoomed, &src_rect, surface, &dest_rect);
  ::SDL_FreeSurface(zoomed);
}

void
Canvas::stretch(const Canvas &src,
                int src_x, int src_y,
                unsigned src_width, unsigned src_height)
{
  // XXX
  stretch(0, 0, get_width(), get_height(),
          src, src_x, src_y, src_width, src_height);
}

static bool
clip_range(int &a, unsigned a_size, int &b, unsigned b_size, unsigned &size)
{
  if (a < 0) {
    b -= a;
    size += a;
    a = 0;
  }

  if (b < 0) {
    a -= b;
    size += b;
    b = 0;
  }

  if ((int)size <= 0)
    return false;

  if (a + size > a_size)
    size = a_size - a;

  if ((int)size <= 0)
    return false;

  if (b + size > b_size)
    size = b_size - b;

  return (int)size > 0;
}

static void
blit_or(SDL_Surface *dest, int dest_x, int dest_y,
        unsigned dest_width, unsigned dest_height,
        SDL_Surface *_src, int src_x, int src_y)
{
  int ret;

  /* obey the dest and src surface borders */

  if (!clip_range(dest_x, dest->w, src_x, _src->w, dest_width) ||
      !clip_range(dest_y, dest->h, src_y, _src->h, dest_height))
    return;

  ret = ::SDL_LockSurface(dest);
  if (ret != 0)
    return;

  /* convert src's pixel format */

  SDL_Surface *src = ::SDL_ConvertSurface(_src, dest->format, SDL_SWSURFACE);
  if (src == NULL) {
    ::SDL_UnlockSurface(dest);
    return;
  }

  ret = ::SDL_LockSurface(src);
  if (ret != 0) {
    ::SDL_FreeSurface(src);
    ::SDL_UnlockSurface(dest);
    return;
  }

  /* get pointers to the upper left dest/src pixel */

  unsigned char *dest_buffer = (unsigned char *)dest->pixels;
  dest_buffer += dest_y * dest->pitch +
    dest_x * dest->format->BytesPerPixel;

  unsigned char *src_buffer = (unsigned char *)src->pixels;
  src_buffer += src_y * src->pitch +
    src_x * src->format->BytesPerPixel;

  /* copy line by line */

  for (unsigned y = 0; y < dest_height; ++y) {
    ::SDL_imageFilterBitOr(src_buffer, dest_buffer, dest_buffer,
                           dest_width * dest->format->BytesPerPixel);
    src_buffer += src->pitch;
    dest_buffer += dest->pitch;
  }

  /* cleanup */

  ::SDL_UnlockSurface(src);
  ::SDL_FreeSurface(src);
  ::SDL_UnlockSurface(dest);
}

static void
blit_and(SDL_Surface *dest, int dest_x, int dest_y,
         unsigned dest_width, unsigned dest_height,
         SDL_Surface *_src, int src_x, int src_y)
{
  int ret;

  /* obey the dest and src surface borders */

  if (!clip_range(dest_x, dest->w, src_x, _src->w, dest_width) ||
      !clip_range(dest_y, dest->h, src_y, _src->h, dest_height))
    return;

  ret = ::SDL_LockSurface(dest);
  if (ret != 0)
    return;

  /* convert src's pixel format */

  SDL_Surface *src = ::SDL_ConvertSurface(_src, dest->format, SDL_SWSURFACE);
  if (src == NULL) {
    ::SDL_UnlockSurface(dest);
    return;
  }

  ret = ::SDL_LockSurface(src);
  if (ret != 0) {
    ::SDL_FreeSurface(src);
    ::SDL_UnlockSurface(dest);
    return;
  }

  /* get pointers to the upper left dest/src pixel */

  unsigned char *dest_buffer = (unsigned char *)dest->pixels;
  dest_buffer += dest_y * dest->pitch +
    dest_x * dest->format->BytesPerPixel;

  unsigned char *src_buffer = (unsigned char *)src->pixels;
  src_buffer += src_y * src->pitch +
    src_x * src->format->BytesPerPixel;

  /* copy line by line */

  for (unsigned y = 0; y < dest_height; ++y) {
    ::SDL_imageFilterBitAnd(src_buffer, dest_buffer, dest_buffer,
                            dest_width * dest->format->BytesPerPixel);
    src_buffer += src->pitch;
    dest_buffer += dest->pitch;
  }

  /* cleanup */

  ::SDL_UnlockSurface(src);
  ::SDL_FreeSurface(src);
  ::SDL_UnlockSurface(dest);
}

void
Canvas::copy_or(int dest_x, int dest_y,
                unsigned dest_width, unsigned dest_height,
                const Canvas &src, int src_x, int src_y)
{
  assert(src.surface != NULL);

  ::blit_or(surface, dest_x, dest_y, dest_width, dest_height,
            src.surface, src_x, src_y);
}

void
Canvas::copy_and(int dest_x, int dest_y,
                 unsigned dest_width, unsigned dest_height,
                 const Canvas &src, int src_x, int src_y)
{
  assert(src.surface != NULL);

  ::blit_and(surface, dest_x, dest_y, dest_width, dest_height,
             src.surface, src_x, src_y);
}

void
Canvas::stretch_or(int dest_x, int dest_y,
                   unsigned dest_width, unsigned dest_height,
                   const Canvas &src,
                   int src_x, int src_y,
                   unsigned src_width, unsigned src_height)
{
  assert(src.surface != NULL);

  SDL_Surface *zoomed =
    ::zoomSurface(src.surface, (double)dest_width / (double)src_width,
                  (double)dest_height / (double)src_height,
                  SMOOTHING_OFF);

  if (zoomed == NULL)
    return;

  ::SDL_SetColorKey(zoomed, 0, 0);

  ::blit_or(surface, dest_x, dest_y, zoomed->w, zoomed->h,
            zoomed,
            (src_x * dest_width) / src_width,
            (src_y * dest_height) / src_height);
  ::SDL_FreeSurface(zoomed);
}

void
Canvas::stretch_and(int dest_x, int dest_y,
                    unsigned dest_width, unsigned dest_height,
                    const Canvas &src,
                    int src_x, int src_y,
                    unsigned src_width, unsigned src_height)
{
  assert(src.surface != NULL);

  SDL_Surface *zoomed =
    ::zoomSurface(src.surface, (double)dest_width / (double)src_width,
                  (double)dest_height / (double)src_height,
                  SMOOTHING_OFF);

  if (zoomed == NULL)
    return;

  ::SDL_SetColorKey(zoomed, 0, 0);

  ::blit_and(surface, dest_x, dest_y, zoomed->w, zoomed->h,
             zoomed,
             (src_x * dest_width) / src_width,
             (src_y * dest_height) / src_height);
  ::SDL_FreeSurface(zoomed);
}

#else /* !ENABLE_SDL */

// TODO: ClipPolygon is not thread safe (uses a static array)!
// We need to make it so.

void
Canvas::clipped_polygon(const POINT* lppt, unsigned cPoints)
{
  ::ClipPolygon(*this, lppt, cPoints, true);
}

void
Canvas::clipped_polyline(const POINT* lppt, unsigned cPoints)
{
  ::ClipPolygon(*this, lppt, cPoints, false);
}

void
Canvas::autoclip_polygon(const POINT* lppt, unsigned cPoints)
{
  if (need_clipping())
    clipped_polygon(lppt, cPoints);
  else
    polygon(lppt, cPoints);
}

void
Canvas::autoclip_polyline(const POINT* lppt, unsigned cPoints)
{
  if (need_clipping())
    clipped_polyline(lppt, cPoints);
  else
    polyline(lppt, cPoints);
}

void
Canvas::line(int ax, int ay, int bx, int by)
{
#ifndef NOLINETO
  ::MoveToEx(dc, ax, ay, NULL);
  ::LineTo(dc, bx, by);
#else
  POINT p[2] = {{ax, ay}, {bx, by}};
  polyline(p, 2);
#endif
}

void
Canvas::two_lines(int ax, int ay, int bx, int by, int cx, int cy)
{
#ifndef NOLINETO
  ::MoveToEx(dc, ax, ay, NULL);
  ::LineTo(dc, bx, by);
  ::LineTo(dc, cx, cy);
#else
  POINT p[2];

  p[0].x = ax;
  p[0].y = ay;
  p[1].x = bx;
  p[1].y = by;
  polyline(p, 2);

  p[0].x = cx;
  p[0].y = cy;
  polyline(p, 2);
#endif
}

void
Canvas::move_to(int x, int y)
{
#ifdef NOLINETO
  cursor.x = x;
  cursor.y = y;
#else
  ::MoveToEx(dc, x, y, NULL);
#endif
}

void
Canvas::line_to(int x, int y)
{
#ifdef NOLINETO
  line(cursor.x, cursor.y, x, y);
  move_to(x, y);
#else
  ::LineTo(dc, x, y);
#endif
}

void
Canvas::segment(int x, int y, unsigned radius,
                Angle start, Angle end, bool horizon)
{
  ::Segment(*this, x, y, radius, start, end, horizon);
}

const SIZE
Canvas::text_size(const TCHAR *text, size_t length) const
{
  SIZE size;
  ::GetTextExtentPoint(dc, text, length, &size);
  return size;
}

const SIZE
Canvas::text_size(const TCHAR *text) const
{
  return text_size(text, _tcslen(text));
}

unsigned
Canvas::text_height(const TCHAR *text) const
{
  TEXTMETRIC tm;
  GetTextMetrics(dc, &tm);
  return tm.tmHeight;
}

void
Canvas::text(int x, int y, const TCHAR *text)
{
  ::ExtTextOut(dc, x, y, 0, NULL, text, _tcslen(text), NULL);
}

void
Canvas::text(int x, int y, const TCHAR *text, size_t length)
{
  ::ExtTextOut(dc, x, y, 0, NULL, text, length, NULL);
}

void
Canvas::text_opaque(int x, int y, const RECT &rc, const TCHAR *text)
{
  ::ExtTextOut(dc, x, y, ETO_OPAQUE, &rc, text, _tcslen(text), NULL);
}

void
Canvas::text_clipped(int x, int y, const RECT &rc, const TCHAR *text)
{
  ::ExtTextOut(dc, x, y, ETO_CLIPPED, &rc, text, _tcslen(text), NULL);
}

void
Canvas::text_clipped(int x, int y, unsigned width, const TCHAR *text)
{
  const SIZE size = text_size(text);

  RECT rc;
  ::SetRect(&rc, x, y, x + min(width, (unsigned)size.cx), y + size.cy);
  text_clipped(x, y, rc, text);
}

void
Canvas::bottom_right_text(int x, int y, const TCHAR *text)
{
  size_t length = _tcslen(text);
  SIZE size;

  // XXX use SetTextAlign() instead?
  ::GetTextExtentPoint(dc, text, length, &size);
  ::ExtTextOut(dc, x - size.cx, y - size.cy,
               0, NULL, text, length, NULL);
}

void
Canvas::copy(int dest_x, int dest_y,
             unsigned dest_width, unsigned dest_height,
             const Canvas &src, int src_x, int src_y)
{
  ::BitBlt(dc, dest_x, dest_y, dest_width, dest_height,
           src.dc, src_x, src_y, SRCCOPY);
}

void
Canvas::copy(const Canvas &src, int src_x, int src_y)
{
  copy(0, 0, width, height, src, src_x, src_y);
}

void
Canvas::copy(const Canvas &src)
{
  copy(src, 0, 0);
}

void
Canvas::copy_transparent_black(const Canvas &src)
{
#ifdef _WIN32_WCE
  ::TransparentImage(dc, 0, 0, get_width(), get_height(),
                     src.dc, 0, 0, get_width(), get_height(),
                     Color::BLACK);
#else
  ::TransparentBlt(dc, 0, 0, get_width(), get_height(),
                   src.dc, 0, 0, get_width(), get_height(),
                   Color::BLACK);
#endif
}

void
Canvas::copy_transparent_white(const Canvas &src)
{
#ifdef _WIN32_WCE
  ::TransparentImage(dc, 0, 0, get_width(), get_height(),
                     src.dc, 0, 0, get_width(), get_height(),
                     Color::WHITE);
#else
  ::TransparentBlt(dc, 0, 0, get_width(), get_height(),
                   src.dc, 0, 0, get_width(), get_height(),
                   Color::WHITE);
#endif
}

void
Canvas::stretch_transparent(const Canvas &src, Color key)
{
#ifdef _WIN32_WCE
  ::TransparentImage(dc, 0, 0, get_width(), get_height(),
                     src.dc, 0, 0, src.get_width(), src.get_height(),
                     key);
#else
  ::TransparentBlt(dc, 0, 0, get_width(), get_height(),
                   src.dc, 0, 0, src.get_width(), src.get_height(),
                   key);
#endif
}

void
Canvas::stretch(int dest_x, int dest_y,
                unsigned dest_width, unsigned dest_height,
                const Canvas &src,
                int src_x, int src_y,
                unsigned src_width, unsigned src_height)
{
  ::StretchBlt(dc, dest_x, dest_y, dest_width, dest_height,
               src.dc, src_x, src_y, src_width, src_height,
               SRCCOPY);
}

void
Canvas::stretch(const Canvas &src,
                int src_x, int src_y,
                unsigned src_width, unsigned src_height)
{
  stretch(0, 0, width, height, src, src_x, src_y, src_width, src_height);
}

void
Canvas::copy_or(int dest_x, int dest_y,
                unsigned dest_width, unsigned dest_height,
                const Canvas &src, int src_x, int src_y)
{
  ::BitBlt(dc, dest_x, dest_y, dest_width, dest_height,
           src.dc, src_x, src_y, SRCPAINT);
}

void
Canvas::copy_and(int dest_x, int dest_y,
                 unsigned dest_width, unsigned dest_height,
                 const Canvas &src, int src_x, int src_y)
{
  ::BitBlt(dc, dest_x, dest_y, dest_width, dest_height,
           src.dc, src_x, src_y, SRCAND);
}

void
Canvas::stretch_or(int dest_x, int dest_y,
                   unsigned dest_width, unsigned dest_height,
                   const Canvas &src,
                   int src_x, int src_y,
                   unsigned src_width, unsigned src_height)
{
  ::StretchBlt(dc, dest_x, dest_y, dest_width, dest_height,
               src.dc, src_x, src_y, src_width, src_height,
               SRCPAINT);
}

void
Canvas::stretch_and(int dest_x, int dest_y,
                    unsigned dest_width, unsigned dest_height,
                    const Canvas &src,
                    int src_x, int src_y,
                    unsigned src_width, unsigned src_height)
{
  ::StretchBlt(dc, dest_x, dest_y, dest_width, dest_height,
               src.dc, src_x, src_y, src_width, src_height,
               SRCAND);
}

#endif /* !ENABLE_SDL */

void
Canvas::stretch(const Canvas &src)
{
  stretch(src, 0, 0, src.get_width(), src.get_height());
}

void
Canvas::scale_copy(int dest_x, int dest_y,
                   const Canvas &src,
                   int src_x, int src_y,
                   unsigned src_width, unsigned src_height)
{
  if (Layout::ScaleEnabled())
    stretch(dest_x, dest_y,
            Layout::Scale(src_width), Layout::Scale(src_height),
            src, src_x, src_y, src_width, src_height);
  else
    copy(dest_x, dest_y, src_width, src_height,
            src, src_x, src_y);
}

void
Canvas::scale_or(int dest_x, int dest_y,
                 const Canvas &src,
                 int src_x, int src_y,
                 unsigned src_width, unsigned src_height)
{
  if (Layout::ScaleEnabled())
    stretch_or(dest_x, dest_y,
               Layout::Scale(src_width), Layout::Scale(src_height),
               src, src_x, src_y, src_width, src_height);
  else
    copy_or(dest_x, dest_y, src_width, src_height,
            src, src_x, src_y);
}

void
Canvas::scale_and(int dest_x, int dest_y,
                  const Canvas &src,
                  int src_x, int src_y,
                  unsigned src_width, unsigned src_height)
{
  if (Layout::ScaleEnabled())
    stretch_and(dest_x, dest_y,
                Layout::Scale(src_width), Layout::Scale(src_height),
                src, src_x, src_y, src_width, src_height);
  else
    copy_and(dest_x, dest_y, src_width, src_height,
             src, src_x, src_y);
}
