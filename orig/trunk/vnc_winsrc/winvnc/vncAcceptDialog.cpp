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
// If the source code for the VNC system is not available from the place 
// whence you received this file, check http://www.uk.research.att.com/vnc or contact
// the authors on vnc@uk.research.att.com for information on obtaining it.


// vncAcceptDialog.cpp: implementation of the vncAcceptDialog class, used
// to query whether or not to accept incoming connections.

#include "stdhdrs.h"
#include "vncAcceptDialog.h"
#include "WinVNC.h"

#include "resource.h"

// Constructor

vncAcceptDialog::vncAcceptDialog(UINT timeoutSecs,
								 BOOL acceptOnTimeout,
								 BOOL allowNoPass,
								 const char *ipAddress)
{
	m_timeoutSecs = timeoutSecs;
	m_acceptOnTimeout = acceptOnTimeout;
	m_allowNoPass = allowNoPass;
	m_ipAddress = strdup(ipAddress);
}

// Destructor

vncAcceptDialog::~vncAcceptDialog()
{
	if (m_ipAddress)
		free(m_ipAddress);
}

// Routine called to activate the dialog and, once it's done, delete it

int vncAcceptDialog::DoDialog()
{
	int retVal = DialogBoxParam(hAppInstance, MAKEINTRESOURCE(IDD_ACCEPT_CONN), 
		NULL, (DLGPROC) vncAcceptDlgProc, (LONG) this);
	delete this;
	switch (retVal)
	{
		case IDACCEPT:
			return 1;
		case IDACCEPT_NOPASS:
			return 2;
		case IDREJECT:
			return 0;
	}
	return 0;
}

// Callback function - handles messages sent to the dialog box

BOOL CALLBACK vncAcceptDialog::vncAcceptDlgProc(HWND hwnd,
											UINT uMsg,
											WPARAM wParam,
											LPARAM lParam) {
	// This is a static method, so we don't know which instantiation we're 
	// dealing with. But we can get a pseudo-this from the parameter to 
	// WM_INITDIALOG, which we therafter store with the window and retrieve
	// as follows:
	vncAcceptDialog *_this = (vncAcceptDialog *) GetWindowLong(hwnd, GWL_USERDATA);

	switch (uMsg) {

		// Dialog has just been created
	case WM_INITDIALOG:
		{
			// Save the lParam into our user data so that subsequent calls have
			// access to the parent C++ object

            SetWindowLong(hwnd, GWL_USERDATA, lParam);
            vncAcceptDialog *_this = (vncAcceptDialog *) lParam;

			// Disable the "Accept without password" button if needed
			HWND hNoPass = GetDlgItem(hwnd, IDACCEPT_NOPASS);
			EnableWindow(hNoPass, _this->m_allowNoPass);

			// Set the IP-address string
			SetDlgItemText(hwnd, IDC_ACCEPT_IP, _this->m_ipAddress);
			if (SetTimer(hwnd, 1, 1000, NULL) == 0)
			{
				if (_this->m_acceptOnTimeout)
					EndDialog(hwnd, IDACCEPT);
				else
				EndDialog(hwnd, IDREJECT);
			}
			_this->m_timeoutCount = _this->m_timeoutSecs;
			// Update the displayed count
			char temp[256];
			if (_this->m_acceptOnTimeout)
				sprintf(temp, "AutoAccept:%u", (_this->m_timeoutCount));
			else
				sprintf(temp, "AutoReject:%u", (_this->m_timeoutCount));
			SetDlgItemText(hwnd, IDC_ACCEPT_TIMEOUT, temp);

			SetForegroundWindow(hwnd);

			MessageBeep(MB_OK);
            
			SetForegroundWindow(hwnd);

            // Return false to prevent accept button from gaining
			// focus.
			return FALSE;
		}

		// Timer event
	case WM_TIMER:
		if ((_this->m_timeoutCount) == 0) {
			if ( _this->m_acceptOnTimeout ) {
				EndDialog(hwnd, IDACCEPT);
			} else {
				EndDialog(hwnd, IDREJECT);
			}
		}
		_this->m_timeoutCount--;

		// Update the displayed count
		char temp[256];
		if ( _this->m_acceptOnTimeout )
			sprintf(temp, "AutoAccept: %u", (_this->m_timeoutCount));
		else
			sprintf(temp, "AutoReject: %u", (_this->m_timeoutCount));
		SetDlgItemText(hwnd, IDC_ACCEPT_TIMEOUT, temp);
		break;

		// Dialog has just received a command
	case WM_COMMAND:
		switch (LOWORD(wParam)) {

			// User clicked Accept or pressed return
		case IDACCEPT:
		case IDOK:
			EndDialog(hwnd, IDACCEPT);
			return TRUE;

		case IDACCEPT_NOPASS:
			EndDialog(hwnd, IDACCEPT_NOPASS);
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

