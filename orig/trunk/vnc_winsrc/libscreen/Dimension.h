//  Copyright (C) 2008 GlavSoft LLC. All Rights Reserved.
//
//  This file is part of the TightVNC software.
//
//  TightVNC is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
//  USA.
//
// TightVNC homepage on the Web: http://www.tightvnc.com/

#ifndef __DIMENSION_H__
#define __DIMENSION_H__

#include "Rect.h"

class Dimension
{
public:
  Dimension(void) : width(0), height(0) {}
  Dimension(const int w, const int h) : width(w), height(h) {}
  Dimension(const Rect *r) { width = r->getWidth(); height = r->getHeight(); }

  virtual ~Dimension(void);

  Rect getRect() { Rect r(width, height);
                   return r;
  }

  inline bool cmpDim(const Dimension *dim) { return dim->width == width &&
                                                    dim->height == height; }

  inline bool isEmpty() const { return width <= 0 || height <= 0; }
  inline int area() const { return isEmpty() ? 0 : width * height; }

  inline void clear() { width = height = 0; }

  int width;
  int height;
};

#endif // __DIMENSION_H__
