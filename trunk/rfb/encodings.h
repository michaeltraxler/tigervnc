/* Copyright (C) 2002-2004 RealVNC Ltd.  All Rights Reserved.
 *    
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */
#ifndef __RFB_ENCODINGS_H__
#define __RFB_ENCODINGS_H__

namespace rfb {

  const unsigned int encodingRaw = 0;
  const unsigned int encodingCopyRect = 1;
  const unsigned int encodingRRE = 2;
  const unsigned int encodingCoRRE = 4;
  const unsigned int encodingHextile = 5;
  const unsigned int encodingTight = 7;
  const unsigned int encodingZRLE = 16;

  const unsigned int encodingMax = 255;

  const unsigned int pseudoEncodingXCursor = 0xffffff10;
  const unsigned int pseudoEncodingCursor = 0xffffff11;
  const unsigned int pseudoEncodingDesktopSize = 0xffffff21;

  // TightVNC-specific
  const unsigned int pseudoEncodingLastRect = 0xFFFFFF20;
  const unsigned int pseudoEncodingQualityLevel0 = 0xFFFFFFE0;
  const unsigned int pseudoEncodingQualityLevel9 = 0xFFFFFFE9;
  const unsigned int pseudoEncodingCompressLevel0 = 0xFFFFFF00;
  const unsigned int pseudoEncodingCompressLevel9 = 0xFFFFFF00;

  int encodingNum(const char* name);
  const char* encodingName(unsigned int num);
}
#endif
