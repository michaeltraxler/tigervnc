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

  m_updateHandler = new UpdateHandler;
  if (m_updateHandler == 0) {
    return false;
  }
  m_updateHandler->setOutUpdateListener(this);
  m_updateHandler->execute();

  return true;
}

void WinDesktop::onUpdate(void *pSender)
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
  return LoadCursor(NULL, IDC_ARROW);
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

  UpdateContainer updateContainer;
  {
    AutoLock al(&m_updateListenerCriticalSection);
    m_updateHandler->extract(&updateContainer);
  }

  std::vector<Rect> rects;
  std::vector<Rect>::iterator iRect;
  updateContainer.changedRegion.get_rects(&rects);
  int numRects = updateContainer.changedRegion.numRects();

  if (numRects == 0) {
    return true;
  }

  vncRegion changedRegion;
  changedRegion.assignFromNewFormat(&updateContainer.changedRegion);

  m_server->UpdateRegion(changedRegion);

  m_server->TriggerUpdate();

  return true;
}
