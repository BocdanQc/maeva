////////////////////////////////////////////////////////////////////////////////
// Filename:    MAEVADlg.h
//
// Description: Header file for the MAEVA application Dialog Window
//
// Author:      Daniel Emond, IFT and GLO Department, Laval University
////////////////////////////////////////////////////////////////////////////////

#include "afxwin.h"
#include "MAEVA.h"
#include "EditFloat.h"

////////////////////////////////////////////////////////////////////////////////
// GLOBAL Definitions and Declarations
////////////////////////////////////////////////////////////////////////////////
// -----------------------------------------------------------------------------
// Acquisition buffers definitions
// -----------------------------------------------------------------------------
enum T_CURR_ACQ_BUFF
{
   E_1ST_BUFF,
   E_2ND_BUFF,
   E_MAX_BUFF_NB
};

enum T_STYLUS_TYPE
{
   E_ALUMINIUM,
   E_PLASTIC,
   E_STEEL,
   E_WOOD_PINE
};

////////////////////////////////////////////////////////////////////////////////
// CMAEVADlg dialog
////////////////////////////////////////////////////////////////////////////////
class CMAEVADlg : public CDialog
{
// Construction
public:
	CMAEVADlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_MAEVA_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

public:
   BOOL       FindAllSUB20Devices(void);
   sub_device GetSelectedSUB20Device(void);
   BOOL       StartAccelerometer(sub_handle i_shHandle);
   BOOL       StopAccelerometer(sub_handle i_shHandle);
   BOOL       WriteDataAcqBufferToFile(T_CURR_ACQ_BUFF i_eBuff, int i_iDataCount);
   BOOL       StopAcquisition(void);

   afx_msg void OnCbnSelchangeComboStylusType();
   afx_msg void OnKillFocusEditRPM();
   afx_msg void OnKillFocusEditRadius();
   afx_msg void OnCbnSelchangeComboDataRange();
   afx_msg void OnBnClickedButtonIdleTest();
   afx_msg void OnBnClickedButtonScan();
   afx_msg void OnBnClickedButtonStartAcq();
   afx_msg void OnBnClickedButtonStopAcq();
   afx_msg void OnBnClickedButtonExportData();
   afx_msg void OnBnClickedButtonEvalData();
   afx_msg void OnBnClickedButtonExit();

   afx_msg LRESULT OnTimerMsgDataAcqInProgress(WPARAM wParam,LPARAM lParam);
   afx_msg LRESULT OnTimerMsgDataAcqBufferFull(WPARAM wParam,LPARAM lParam);
   afx_msg LRESULT OnTimerMsgDataAcqAccessError(WPARAM wParam,LPARAM lParam);

public:
   sub_handle m_shHandle;
   HANDLE     m_hTimerQueue;
   HANDLE     m_hTimer;
   BOOL       m_bAcquiringData;

private:
   CString       m_strSurfaceType;
   T_STYLUS_TYPE m_eStylusType;
   double        m_fRPM;
   double        m_fRadius;
   double        m_fSpeed;
   T_RANGE_G_SET m_eDataRangeSetting;
   BOOL          m_bAccelStarted;
   BOOL          m_bFileAlreadyOpened;
   int           m_iFileOverrunEndPtr;
   CTime         m_timeAcqStart;
   CTime         m_timeAcqStop;
   LONGLONG      m_uiElapsedTime;

   CListBox   m_lbSUB20DevicesList;
   CStatic    m_staticXAxis;
   CStatic    m_staticYAxis;
   CStatic    m_staticZAxis;
   CEdit      m_editSurfaceType;
   CComboBox  m_cbStylusType;
   CEditFloat m_editRPM;
   CEditFloat m_editRadius;
   CStatic    m_staticSpeed;
   CComboBox  m_cbDataRangeList;
   CStatic    m_staticDataSize;
   CStatic    m_staticElapsedTime;
   CStatic    m_staticOverrunNb;

   CButton m_buttonIdleTest;
   CButton m_buttonStartAcq;
   CButton m_buttonStopAcq;
   CButton m_buttonExportData;
   CButton m_buttonEvalData;
};
