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

#ifndef __UPDATEHANDLER_H__
#define __UPDATEHANDLER_H__

#include "UpdateContainer.h"
#include "UpdateKeeper.h"
#include "UpdateFilter.h"
#include "libscreen/WindowsScreenGrabber.h"
#include "libscreen/FrameBuffer.h"
#include "thread/AutoLock.h"
#include "UpdateListener.h"
#include "UpdateDetector.h"

class UpdateHandler : public UpdateListener
{
public:
  UpdateHandler(UpdateListener *outUpdateListener);
  ~UpdateHandler(void);

  // The extract() function fills in a UpdateContainer object. 
  // Also, if screen properties (such as resolution, pixel format)
  // has changed the function reconfigures FrameBuffers. The
  // reconfiguration posible the only one function.

  // Parameters: 
  //   updateContainer - pointer to a UpdateContainer object that will be filled,
  void extract(UpdateContainer *updateContainer);

  void setFullUpdateRequested(const rfb::Region *region);

  // Checking a region for updates.
  // Return:
  //   true - if updates presents,
  //   false - if not.
  bool checkForUpdates(rfb::Region *region);

  // Set a region excluded from the region that updates detects.
  void setExcludedRegion(const rfb::Region *excludedRegion);

  // The function provides access to FrameBuffer data.
  // The data usage be able until next extract() function call.
  // Return:
  //   constant pointer to the FrameBuffer object.
  const FrameBuffer *getFrameBuffer() const { return &m_backupFrameBuffer; }

  virtual void onUpdate();

private:
  virtual void executeDetectors();
  virtual void terminateDetectors();

  UpdateListener *m_registerUpdateListener;
  UpdateKeeper *m_updateKeeper;
  UpdateFilter *m_updateFilter;
  UpdateDetector *m_poller;
  UpdateDetector *m_hooks;
  UpdateDetector *m_mouseDetector;
  WindowsScreenGrabber m_screenGrabber;
  FrameBuffer m_backupFrameBuffer;
  CriticalSection m_criticalSection;
};

#endif // __UPDATEHANDLER_H__
