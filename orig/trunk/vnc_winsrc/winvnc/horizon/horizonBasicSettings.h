
//  Copyright (C) 2004 Horizon Wimba. All Rights Reserved.

// $Id$

class horizonBasicSettings ;

#ifndef __HORIZON_BASIC_SETTINGS_H
#define __HORIZON_BASIC_SETTINGS_H

#include "stdhdrs.h"

#include "horizonSettingsDialog.h"
#include "horizonProperties.h"

#include "horizonSharedArea.h"

//
// class definition
//

class horizonBasicSettings : public horizonSettingsDialog 
{
public:
	horizonBasicSettings() : horizonSettingsDialog() {} ;
	~horizonBasicSettings() {}

	bool Show( void ) ;

	static BOOL CALLBACK DialogProc( HWND hwnd, UINT uMsg, 
		WPARAM wParam, LPARAM lParam ) { return TRUE ; } ;

} ;

#endif // __HORIZON_BASIC_SETTINGS_H
