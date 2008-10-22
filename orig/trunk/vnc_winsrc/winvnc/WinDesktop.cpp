//  Copyright (C) 2008 GlavSoft LLC. All Rights Reserved.
//  Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
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

#include "WinDesktop.h"

WinDesktop::WinDesktop()
: m_updateHandler(0),
  m_server(0)
{
}

WinDesktop::~WinDesktop()
{
  delete m_updateHandler;
}

bool WinDesktop::Init(vncServer *server)
{
  m_server = server;

  setNewScreenSize();

  m_updateHandler = new UpdateHandler;
  if (m_updateHandler == 0) {
    return false;
  }
  m_updateHandler->setOutUpdateListener(this);
  m_updateHandler->execute();

  return true;
}

void WinDesktop::onUpdate()
{
  sendUpdate();
}

void WinDesktop::RequestUpdate()
{
  sendUpdate();
}

void WinDesktop::SetClipText(LPSTR text)
{
}

void WinDesktop::TryActivateHooks()
{
}

void WinDesktop::FillDisplayInfo(rfbServerInitMsg *scrInfo)
{
  const FrameBuffer *fb = m_updateHandler->getBackupFrameBuffer();
  PixelFormat pf = fb->getPixelFormat();

  scrInfo->format.bigEndian = pf.bigEndian;
  scrInfo->format.bitsPerPixel  = (CARD8)pf.bitsPerPixel;
  scrInfo->format.redMax        = pf.redMax;
  scrInfo->format.redShift      = (CARD8)pf.redShift;
  scrInfo->format.greenMax      = pf.greenMax;
  scrInfo->format.greenShift    = (CARD8)pf.greenShift;
  scrInfo->format.blueMax       = pf.blueMax;
  scrInfo->format.blueShift     = (CARD8)pf.blueShift;
  scrInfo->format.depth         = (CARD8)pf.colorDepth;
  scrInfo->format.trueColour    = 1;
  scrInfo->format.pad1          = 0;
  scrInfo->format.pad2          = 0;

  scrInfo->framebufferWidth = fb->getDimension().width;
  scrInfo->framebufferHeight = fb->getDimension().height;
}

void WinDesktop::SetLocalInputDisableHook(BOOL enable)
{
}

void WinDesktop::SetLocalInputPriorityHook(BOOL enable)
{
}

BYTE *WinDesktop::MainBuffer()
{
  return (BYTE *)m_updateHandler->getBackupFrameBuffer()->getBuffer();
}

int WinDesktop::ScreenBuffSize()
{
  return m_updateHandler->getBackupFrameBuffer()->getBufferSize();
}

void WinDesktop::CaptureScreen(RECT &UpdateArea, BYTE *scrBuff)
{
}

void WinDesktop::CaptureMouse(BYTE *scrBuff, UINT scrBuffSize)
{
}

RECT WinDesktop::MouseRect()
{
  RECT rect;
  rect.left = 0;
  rect.top = 0;
  rect.right = 0;
  rect.bottom = 0;
  return rect;
}

HCURSOR WinDesktop::GetCursor() const
{
  CURSORINFO cursorInfo;
  cursorInfo.cbSize = sizeof(CURSORINFO);
  GetCursorInfo(&cursorInfo);

  HCURSOR hCursor = cursorInfo.hCursor;
  if (hCursor  == NULL) {
    return NULL;
  }

  return hCursor;
}

BOOL WinDesktop::GetRichCursorData(BYTE *databuf, HCURSOR hcursor, int width, int height)
{
  return TRUE;
}

bool WinDesktop::sendUpdate()
{
  if (!m_server->IncrRgnRequested() && !m_server->FullRgnRequested()) {
    return true;
  }

  m_server->UpdateMouse();

  UpdateContainer updateContainer;

  {
    AutoLock al(&m_updateListenerCriticalSection);
    m_updateHandler->extract(&updateContainer);
  }

  if (updateContainer.isEmpty()) {
    return true;
  }

  if (updateContainer.cursorPosChanged) {
    m_server->UpdateMouse();
  }

  if (updateContainer.screenSizeChanged) {
    setNewScreenSize();
  }

  std::vector<Rect> rects;
  std::vector<Rect>::iterator iRect;
  updateContainer.changedRegion.get_rects(&rects);
  int numRects = updateContainer.changedRegion.numRects();

  if (numRects > 0) {
    vncRegion changedRegion;
    changedRegion.assignFromNewFormat(&updateContainer.changedRegion);
    m_server->UpdateRegion(changedRegion);
  }

  shareRect();
  m_server->TriggerUpdate();

  return true;
}

void WinDesktop::shareRect()
{
  // EXAMINE THE SHARED AREA / WINDOW

  RECT rect = m_server->GetSharedRect();
  RECT new_rect;

  if (m_server->WindowShared()) {
    HWND hwnd = m_server->GetWindowShared();
    GetWindowRect(hwnd, &new_rect);
  } else if (m_server->ScreenAreaShared()) {
    new_rect = m_server->GetScreenAreaRect();
  } else {
    new_rect = m_bmrect;
  }

  if ((m_server->WindowShared() || m_server->GetApplication()) &&
      m_server->GetWindowShared() == NULL) {
    // Disconnect clients if the shared window has dissapeared.
    // FIXME: Make this behavior configurable.
    MessageBox(NULL, "You have exited an application that is being\n"
                     "viewed/controlled from a remote PC. Exiting this\n"
                     "application will terminate the session with the remote PC.",
                     "Warning", MB_ICONWARNING | MB_OK);
    vnclog.Print(LL_CONNERR, VNCLOG("shared window not found - disconnecting clients\n"));
    m_server->KillAuthClients();
    return;
  }

  // intersect the shared rect with the desktop rect
  IntersectRect(&new_rect, &new_rect, &m_bmrect);

  // Disconnect clients if the shared window is empty (dissapeared).
  // FIXME: Make this behavior configurable.
  if (new_rect.right - new_rect.left == 0 ||
      new_rect.bottom - new_rect.top == 0) {
    vnclog.Print(LL_CONNERR, VNCLOG("shared window empty - disconnecting clients\n"));
    m_server->KillAuthClients();
    return;
  }

  // Update screen size if required
  if (!EqualRect(&new_rect, &rect)) {
    m_server->SetSharedRect(new_rect);
    bool sendnewfb = false;

    if (rect.right - rect.left != new_rect.right - new_rect.left ||
        rect.bottom - rect.top != new_rect.bottom - new_rect.top ) {
      sendnewfb = true;
    }

    // FIXME: We should not send NewFBSize if a client
    //        did not send framebuffer update request.
    m_server->SetNewFBSize(sendnewfb);
    return;
  }		
}

void WinDesktop::setNewScreenSize()
{
  m_bmrect.left   = 0;
  m_bmrect.top    = 0;
  m_bmrect.right  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  m_bmrect.bottom = GetSystemMetrics(SM_CYVIRTUALSCREEN);
}