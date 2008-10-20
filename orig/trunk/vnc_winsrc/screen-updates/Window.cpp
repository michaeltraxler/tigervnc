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

#include "Window.h"
#include "TCHAR.h"

#define DEFAULT_WINDOW_CLASS_NAME "WindowClass"

Window::Window(HINSTANCE hinst, TCHAR *windowClassName)
: m_hwnd(0),
m_hinst(hinst),
m_windowClassName(0)
{
  if (windowClassName != 0) {
    m_windowClassName = _tcsdup(windowClassName);
  } else {
    m_windowClassName = _tcsdup(_T(DEFAULT_WINDOW_CLASS_NAME));
  }

}

Window::~Window(void)
{
  if (m_windowClassName != 0) {
    free(m_windowClassName);
  }
  CloseWindow(m_hwnd);
}

bool Window::createWindow()
{
  if (regClass(m_hinst, m_windowClassName) == 0) {
    return false;
  }

  m_hwnd = ::CreateWindow(m_windowClassName, _T("Window"),
                          0, 0, 0, 1, 1, HWND_MESSAGE, NULL, m_hinst, NULL);
  if (m_hwnd == 0) {
    return false;
  }

  SetWindowLong(m_hwnd, GWL_USERDATA, (LONG) this);
  return true;
}

LRESULT CALLBACK Window::staticWndProc(HWND hwnd, UINT message,
                                       WPARAM wParam, LPARAM lParam)
{
  Window *_this;
  _this = (Window *)GetWindowLong(hwnd, GWL_USERDATA);
  if (_this != NULL) {
    if (_this->wndProc(message, wParam, lParam)) {
      return 0;
    }
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

ATOM Window::regClass(HINSTANCE hinst, TCHAR *windowClassName)
{
  WNDCLASS wcWindowClass = {0};
  wcWindowClass.lpfnWndProc = staticWndProc;
  wcWindowClass.style = NULL;
  wcWindowClass.hInstance = m_hinst;
  wcWindowClass.lpszClassName = windowClassName;
  wcWindowClass.hCursor = NULL;
  wcWindowClass.hbrBackground = (HBRUSH)COLOR_WINDOW;

  return RegisterClass(&wcWindowClass);
}
