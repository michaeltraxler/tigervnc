
//  Copyright (C) 2004 Horizon Wimba. All Rights Reserved.

// $Id$

class PollCycleControl ;

#ifndef __POLLCYCLE_CONTROL_H
#define __POLLCYCLE_CONTROL_H

//
// include files
//

#include "stdhdrs.h"

#include "vncServer.h"
#include "vncServerSingleton.h"

#include "CPUUsage.h"
#include "CPUUsageWin32HRPC.h"

//
// class definition
//

class PollCycleControl
{
public:
	static PollCycleControl* GetInstance( void ) ;

	bool Start( void ) ;
	bool Stop( void ) ;

	static VOID CALLBACK TimerProc( HWND hwnd, UINT uMsg, 
		UINT idEvent, DWORD dwTime ) ;

	// CPUUsageWin32HRPC adapter methods
	bool SetPollCycle( unsigned int ms_per_interval ) ;
	bool MarkPollStart( void ) ;
	bool MarkPollStop( void ) ;

private:	
	PollCycleControl() ;
	~PollCycleControl() ;

	PollCycleControl( const PollCycleControl& rhs ) ;
	const PollCycleControl& operator=( const PollCycleControl& rhs ) ;

	static void CalculateScreenRefresh( UINT cycle ) ;

	static CPUUsageWin32HRPC* m_cpu_hrpc ;
	static CPUUsage* m_cpu ;
	
	static UINT m_timer_id ;
	static int m_last_cycle ;
	
	static PollCycleControl* m_instance ;
} ;

#endif // __POLLCYCLE_CONTROL_H
