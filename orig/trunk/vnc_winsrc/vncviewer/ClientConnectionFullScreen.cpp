//  Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//
//  This file is part of the VNC system.
//
//  The VNC system is free software; you can redistribute it and/or modify
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
// TightVNC distribution homepage on the Web: http://www.tightvnc.com/
//
// If the source code for the VNC system is not available from the place 
// whence you received this file, check http://www.uk.research.att.com/vnc or 
// contact the authors on vnc@uk.research.att.com for information on obtaining it.
//
// Many thanks to Greg Hewgill <greg@hewgill.com> for providing the basis for 
// the full-screen mode.

#include "stdhdrs.h"
#include "vncviewer.h"
#include "ClientConnection.h"

// Parameters for scrolling in full screen mode
#define BUMPSCROLLBORDER 4
#define BUMPSCROLLAMOUNTX 16
#define BUMPSCROLLAMOUNTY 4

bool ClientConnection::InFullScreenMode() 
{
	return m_opts.m_FullScreen; 
};

// You can explicitly change mode by calling this
void ClientConnection::SetFullScreenMode(bool enable)
{	
	m_opts.m_FullScreen = enable;
	RealiseFullScreenMode(false);
}

LRESULT CALLBACK ClientConnection::DisableDlgProc(HWND hwnd, UINT iMsg,
												  WPARAM wParam, LPARAM lParam)
{
	// We use the dialog-box's USERDATA to store a _this pointer
	// This is set only once WM_INITDIALOG has been recieved, though!
	ClientConnection *_this = (ClientConnection *) GetWindowLong(hwnd, GWL_USERDATA);

	switch (iMsg) {
	case WM_INITDIALOG: 
		{
			// Retrieve the Dialog box parameter and use it as a pointer
			// to the calling VNCOptions object
			SetWindowLong(hwnd, GWL_USERDATA, lParam);
			ClientConnection *_this = (ClientConnection *) lParam;
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))	{
		case IDC_DISABLE:
			_this->SetFullScreenMode(false);
			DestroyWindow(_this->m_hdisable);
			_this->m_hdisable = NULL;
			return TRUE;
		}
		return FALSE;
	}
	return 0;
}

// If the options have been changed other than by calling 
// SetFullScreenMode, you need to call this to make it happen.
void ClientConnection::RealiseFullScreenMode(bool suppressPrompt)
{
	LONG style = GetWindowLong(m_hwnd1, GWL_STYLE);
	if (m_opts.m_FullScreen) {
		if (!suppressPrompt && !pApp->m_options.m_skipprompt) {
			MessageBox(m_hwnd1, 
				_T("To exit from full-screen mode, use Ctrl-Esc Esc and then\r\n"
				"right-click on the vncviewer taskbar icon to see the menu."),
				_T("VNCviewer full-screen mode"),
				MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
		}
		ShowWindow(m_hToolbar, SW_HIDE);
		EnableMenuItem(GetSystemMenu(m_hwnd1, FALSE), ID_TOOLBAR, MF_BYCOMMAND|MF_GRAYED);
		ShowWindow(m_hwnd1, SW_MAXIMIZE);
		style = GetWindowLong(m_hwnd1, GWL_STYLE);
		style &= ~(WS_DLGFRAME | WS_THICKFRAME);
		
		SetWindowLong(m_hwnd1, GWL_STYLE, style);
		int cx = GetSystemMetrics(SM_CXSCREEN);
		int cy = GetSystemMetrics(SM_CYSCREEN);
		SetWindowPos(m_hwnd1, HWND_TOPMOST, -1, -1, cx + 3, cy + 3, SWP_FRAMECHANGED);
		CheckMenuItem(GetSystemMenu(m_hwnd1, FALSE), ID_FULLSCREEN, MF_BYCOMMAND|MF_CHECKED);
		m_hdisable = CreateDialogParam(pApp->m_instance, MAKEINTRESOURCE(IDD_FULL_SCREEN_DISABLE),
					m_hwndscroll, (DLGPROC)DisableDlgProc, (LONG)this);
	} else {
		ShowWindow(m_hToolbar, SW_SHOW);
		EnableMenuItem(GetSystemMenu(m_hwnd1, FALSE), ID_TOOLBAR, MF_BYCOMMAND|MF_ENABLED);
		style |= (WS_DLGFRAME | WS_THICKFRAME);
		
		SetWindowLong(m_hwnd1, GWL_STYLE, style);
		SetWindowPos(m_hwnd1, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		ShowWindow(m_hwnd1, SW_NORMAL);		
		CheckMenuItem(GetSystemMenu(m_hwnd1, FALSE), ID_FULLSCREEN, MF_BYCOMMAND|MF_UNCHECKED);

	}
}

bool ClientConnection::BumpScroll(int x, int y)
{
	int dx = 0;
	int dy = 0;
	int rightborder;
	int bottomborder;
	if (!InFullScreenMode()) {
		RECT rect;
		GetClientRect(m_hwnd, &rect);
		rightborder	= rect.right-BUMPSCROLLBORDER;
		bottomborder = rect.bottom-BUMPSCROLLBORDER;
	} else {
		rightborder	= GetSystemMetrics(SM_CXSCREEN)-BUMPSCROLLBORDER;
		bottomborder = GetSystemMetrics(SM_CYSCREEN)-BUMPSCROLLBORDER;
	}
	if (x < BUMPSCROLLBORDER)
		dx = -BUMPSCROLLAMOUNTX * m_opts.m_scale_num / m_opts.m_scale_den;
	if (x >= rightborder)
		dx = +BUMPSCROLLAMOUNTX * m_opts.m_scale_num / m_opts.m_scale_den;;
	if (y < BUMPSCROLLBORDER)
		dy = -BUMPSCROLLAMOUNTY * m_opts.m_scale_num / m_opts.m_scale_den;;
	if (y >= bottomborder)
		dy = +BUMPSCROLLAMOUNTY * m_opts.m_scale_num / m_opts.m_scale_den;;
	if (dx || dy) {
		if (ScrollScreen(dx,dy)) {
			// If we haven't physically moved the cursor, artificially
			// generate another mouse event so we keep scrolling.
			POINT p;
			GetCursorPos(&p);			
			ScreenToClient(m_hwnd, &p);			
			if (p.x == x && p.y == y) {
				ClientToScreen(m_hwnd, &p);
				SetCursorPos(p.x,p.y);
			}
			return true;
		} 
	}
	return false;
}
