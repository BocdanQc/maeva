////////////////////////////////////////////////////////////////////////////////
// Filename:    MAEVA.cpp
//
// Description: Defines the class behaviors for the application. 
//
// Author:      Daniel Emond, IFT and GLO Department, Laval University
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MAEVA.h"
#include "MAEVADlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMAEVAApp

BEGIN_MESSAGE_MAP(CMAEVAApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CMAEVAApp construction

CMAEVAApp::CMAEVAApp()
{

}

// The one and only CMAEVAApp object

CMAEVAApp theApp;


// CMAEVAApp initialization

BOOL CMAEVAApp::InitInstance()
{
	CWinApp::InitInstance();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	SetRegistryKey(_T("Robotics Lab - IFT and GLO Department - Laval University"));

	CMAEVADlg dlg;
	m_pMainWnd = &dlg;

   timeBeginPeriod(1);
   SetThreadPriority(THREAD_PRIORITY_HIGHEST);
	INT_PTR nResponse = dlg.DoModal();
   SetThreadPriority(THREAD_PRIORITY_NORMAL);
   timeEndPeriod(1); 

	// Since the dialog has been closed, return FALSE so that we exit the
	// application, rather than start the application's message pump.

   return FALSE;
}
