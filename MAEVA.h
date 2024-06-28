////////////////////////////////////////////////////////////////////////////////
// Filename:    MAEVA.h
//
// Description: Main header file for the MAEVA application 
//
// Author:      Daniel Emond, IFT and GLO Department, Laval University
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CMAEVAApp:
// See MAEVA.cpp for the implementation of this class
//

class CMAEVAApp : public CWinApp
{
public:
	CMAEVAApp();

// Overrides
	public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CMAEVAApp theApp;