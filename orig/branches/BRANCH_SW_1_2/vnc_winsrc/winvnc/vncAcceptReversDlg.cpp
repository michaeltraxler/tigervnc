//  Copyright (C) 2002 HorizonLive.com, Inc. All Rights Reserved.
//  Copyright (C) 2000 Tridia Corporation. All Rights Reserved.
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
// whence you received this file, check http://www.uk.research.att.com/vnc or contact
// the authors on vnc@uk.research.att.com for information on obtaining it.


// vncAcceptReversDlg.cpp

#include "stdhdrs.h"
#include "vncAcceptReversDlg.h"
#include "WinVNC.h"
#include "vncService.h"
#include "resource.h"

// Constructor

vncAcceptReversDlg::vncAcceptReversDlg(vncMenu *menu, const char *addr)
{
m_menu = menu;
m_ipAddress = strdup(addr);
}

// Destructor

vncAcceptReversDlg::~vncAcceptReversDlg()
{
	if (m_ipAddress)
		free(m_ipAddress);
}

// Routine called to activate the dialog and, once it's done, delete it

int vncAcceptReversDlg::DoDialog()
{
	int retVal = DialogBoxParam(hAppInstance, MAKEINTRESOURCE(IDD_ACCEPT_REVERS), 
		NULL, (DLGPROC) vncAcceptReversDlgProc, (LONG) this);
	delete this;
	switch (retVal)
	{
		case IDACCEPT:
			return true;
		case IDREJECT:
			return false;
	}
	return 0;
}

// Callback function - handles messages sent to the dialog box

BOOL CALLBACK vncAcceptReversDlg::vncAcceptReversDlgProc(HWND hwnd,
											UINT uMsg,
											WPARAM wParam,
											LPARAM lParam) {
	// This is a static method, so we don't know which instantiation we're 
	// dealing with. But we can get a pseudo-this from the parameter to 
	// WM_INITDIALOG, which we therafter store with the window and retrieve
	// as follows:
	vncAcceptReversDlg *_this = (vncAcceptReversDlg *) GetWindowLong(hwnd, GWL_USERDATA);

	switch (uMsg) {

		// Dialog has just been created
	case WM_INITDIALOG:
		{
			// Save the lParam into our user data so that subsequent calls have
			// access to the parent C++ object

            SetWindowLong(hwnd, GWL_USERDATA, lParam);
            vncAcceptReversDlg *_this = (vncAcceptReversDlg *) lParam;

			
			// Set the IP-address string
			char temp[256];
			sprintf(temp, "Would you like to share your computer now via LiveShare at http://%s", (_this->m_ipAddress));
			SetDlgItemText(hwnd, IDC_STATIC_TEXT2, temp);

			SetForegroundWindow(hwnd);

            // Return false to prevent accept button from gaining
			// focus.
			return true;
		}

		case WM_COMMAND:
		
			switch (LOWORD(wParam)) {

			// User clicked Accept or pressed return
		case IDACCEPT:
		case IDOK:
			EndDialog(hwnd, IDACCEPT);
			return TRUE;

		case IDSETTINGS:
			_this->m_menu->m_properties.Show(true,true);
			return TRUE;

		case IDREJECT:
		case IDCANCEL:
			EndDialog(hwnd, IDREJECT);
			return TRUE;
		};

		break;

		// Window is being destroyed!  (Should never happen)
	case WM_DESTROY:
		EndDialog(hwnd, IDREJECT);
		return TRUE;
	}
	return 0;
}

