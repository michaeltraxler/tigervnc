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


#include "stdhdrs.h"
#include "vncviewer.h"
#include "Exception.h"
#ifdef UNDER_CE
#include "omnithreadce.h"
#else
#include "omnithread.h"
#include "VNCviewerApp32.h"
#endif

// All logging is done via the log object
Log vnclog;
HWND hwndd;

HACCEL hAccel;
HACCEL hAccel1;
ACCEL Accel[7];
#ifdef UNDER_CE
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR szCmdLine, int iCmdShow)
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
#endif
{
	// The state of the application as a whole is contained in the one app object
	#ifdef _WIN32_WCE
		VNCviewerApp app(hInstance, szCmdLine);
	#else
		VNCviewerApp32 app(hInstance, szCmdLine);
	#endif

	// Start a new connection if specified on command line, 
	// or if not in listening mode
	
	if (app.m_options.m_connectionSpecified) {
		app.NewConnection(app.m_options.m_host, app.m_options.m_port);
	} else if (!app.m_options.m_listening) {
		// This one will also read from config file if specified
		app.NewConnection();
	}

	MSG msg;
	

	Accel[0].fVirt=FVIRTKEY|FALT|FCONTROL|FSHIFT|FNOINVERT;
	Accel[0].key=0x4f;
	Accel[0].cmd=IDC_OPTIONBUTTON;

	Accel[1].fVirt=FVIRTKEY|FALT|FCONTROL|FSHIFT|FNOINVERT;
	Accel[1].key=0x49;
	Accel[1].cmd=ID_CONN_ABOUT;

	Accel[2].fVirt=FVIRTKEY|FALT|FCONTROL|FSHIFT|FNOINVERT;
	Accel[2].key=0x46;
	Accel[2].cmd=ID_FULLSCREEN;

	Accel[3].fVirt=FVIRTKEY|FALT|FCONTROL|FSHIFT|FNOINVERT;
	Accel[3].key=0x52;
	Accel[3].cmd=ID_REQUEST_REFRESH;

	Accel[4].fVirt=FVIRTKEY|FALT|FCONTROL|FSHIFT|FNOINVERT;
	Accel[4].key=0x4e;
	Accel[4].cmd=ID_NEWCONN;

	Accel[5].fVirt=FVIRTKEY|FALT|FCONTROL|FSHIFT|FNOINVERT;
	Accel[5].key=0x53;
	Accel[5].cmd=ID_CONN_SAVE_AS;

	Accel[6].fVirt=FVIRTKEY|FALT|FCONTROL|FSHIFT|FNOINVERT;
	Accel[6].key=0x54;
	Accel[6].cmd=ID_TOOLBAR;

	hAccel=CreateAcceleratorTable((LPACCEL)Accel,7);
   
	try {
		while ( GetMessage(&msg, NULL, 0,0) ) {
			if(!hAccel||!TranslateAccelerator(hwndd,hAccel,&msg)){
				TranslateMessage(&msg);
			}
			DispatchMessage(&msg);
		} 
	} catch (WarningException &e) {
		e.Report();
	} catch (QuietException &e) {
		e.Report();
	}
	
	// Clean up winsock
	WSACleanup();

	vnclog.Print(3, _T("Exiting\n"));

	return msg.wParam;
}


// Move the given window to the centre of the screen
// and bring it to the top.
void CentreWindow(HWND hwnd)
{
	RECT winrect, workrect;
	
	// Find how large the desktop work area is
	SystemParametersInfo(SPI_GETWORKAREA, 0, &workrect, 0);
	int workwidth = workrect.right -  workrect.left;
	int workheight = workrect.bottom - workrect.top;
	
	// And how big the window is
	GetWindowRect(hwnd, &winrect);
	int winwidth = winrect.right - winrect.left;
	int winheight = winrect.bottom - winrect.top;
	// Make sure it's not bigger than the work area
	winwidth = min(winwidth, workwidth);
	winheight = min(winheight, workheight);

	// Now centre it
	SetWindowPos(hwnd, 
		HWND_TOP,
		workrect.left + (workwidth-winwidth) / 2,
		workrect.top + (workheight-winheight) / 2,
		winwidth, winheight, 
		SWP_SHOWWINDOW);
	SetForegroundWindow(hwnd);
}

// Convert "host:display" or "host::port" into host and port
// Returns true if valid format, false if not.
// Takes initial string, addresses of results and size of host buffer in wchars.
// If the display info passed in is longer than the size of the host buffer, it
// is assumed to be invalid, so false is returned.
// If the function returns true, then it also replaces the display[]
// string with its canonical representation.
bool ParseDisplay(LPTSTR display, LPTSTR phost, int hostlen, int *pport) 
{
    if (hostlen < (int)_tcslen(display))
        return false;

    int tmp_port;
    TCHAR *colonpos = _tcschr(display, L':');
    if (colonpos == NULL) {
		// No colon -- use default port number
        tmp_port = RFB_PORT_OFFSET;
		_tcsncpy(phost, display, MAX_HOST_NAME_LEN);
	} else {
		_tcsncpy(phost, display, colonpos - display);
		phost[colonpos - display] = L'\0';
		if (colonpos[1] == L':') {
			// Two colons -- interpret as a port number
			if (_stscanf(colonpos + 2, TEXT("%d"), &tmp_port) != 1) 
				return false;
		} else {
			// One colon -- interpret as a display number
			if (_stscanf(colonpos + 1, TEXT("%d"), &tmp_port) != 1) 
				return false;
			tmp_port += RFB_PORT_OFFSET;
		}
	}
    *pport = tmp_port;

	// FIXME: We should not overwrite display[] here, buffer overflow
	// is possible.
	if (tmp_port == 5900) {
		_tcscpy(display, phost);
	} else if (tmp_port > 5900 && tmp_port <= 5999) {
		_stprintf(display, TEXT("%s:%d"), phost, tmp_port - 5900);
	} else {
		_stprintf(display, TEXT("%s::%d"), phost, tmp_port);
	}

    return true;
}


