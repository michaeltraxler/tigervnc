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

#ifndef __WINDESKTOP_H__
#define __WINDESKTOP_H__

class WinDesktop;

#include "stdhdrs.h"

#include "vncServer.h"
#include "translate.h"

#include "screen-updates/UpdateHandler.h"
#include "screen-updates/UpdateListener.h"
#include "libscreen/WindowsMouseGrabber.h"
#include "thread/AutoLock.h"
#include "system/DynamicLibrary.h"

class WinDesktop : public UpdateListener, public Thread
{
public:
  WinDesktop();
  virtual ~WinDesktop();

  bool Init(vncServer *server);
  void RequestUpdate();
  void SetClipText(LPSTR text);
  void TryActivateHooks();
  void FillDisplayInfo(rfbServerInitMsg *scrInfo);
  void SetLocalInputDisableHook(BOOL enable);
  void SetLocalInputPriorityHook(BOOL enable);
  BYTE *MainBuffer();
  int ScreenBuffSize();
  const CursorShape *getCursorShape() const { return m_updateHandler->getCursorShape(); }

  RECT getBMRect() const { return m_bmrect; }

protected:
  virtual void onUpdate();

  void winDesktopNotify()
  {
    if (m_hEvent != 0) {
      SetEvent(m_hEvent);
    }
  }

  virtual void execute();
  virtual void onTerminate();

  bool sendUpdate();

  void setNewScreenSize();

  void updateBufferNotify();

  // Check if the desktop handle has been changed and restart.
  bool checkCurrentDesktop(bool *changed);

  UpdateHandler *m_updateHandler;
  WindowsMouseGrabber m_mouseGrabber;
  PixelFormat m_pixelFormat;
  vncServer *m_server;
  RECT m_bmrect;

  // Hooks
  DynamicLibrary *m_dynamicLibrary;
  FARPROC m_setKeyboardFilterHook;
  FARPROC m_setMouseFilterHook;

  HANDLE m_hEvent;
};

#endif // __WINDESKTOP_H__
