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

#ifndef __HOOKSUPDATEDETECTOR_H__
#define __HOOKSUPDATEDETECTOR_H__

#include "UpdateDetector.h"

#define LIBRARY_NAME "VNCHooks.dll"

class HooksUpdateDetector : public UpdateDetector
{
public:
  HooksUpdateDetector(UpdateKeeper *updateKeeper,
                      CriticalSection *updateKeeperCriticalSection);
  virtual ~HooksUpdateDetector(void);

protected:
  virtual void execute();

  CriticalSection *m_updateKeeperCriticalSection;
  HMODULE m_hHooks;
};

#endif // __HOOKSUPDATEDETECTOR_H__
