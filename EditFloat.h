////////////////////////////////////////////////////////////////////////////////
// Filename:    EditFloat.h
//
// Description: Header file for the CEditFloat class derived from CEdit.
//              Allows float numbers only into the edit box.
//
// Author:      Daniel Emond, IFT and GLO Department, Laval University
////////////////////////////////////////////////////////////////////////////////

#pragma once

// CEditFloat

////////////////////////////////////////////////////////////////////////////////
// CEditFloat dialog
////////////////////////////////////////////////////////////////////////////////
class CEditFloat : public CEdit
{
	DECLARE_DYNAMIC(CEditFloat)

public:
	CEditFloat();
	virtual ~CEditFloat();

protected:
	DECLARE_MESSAGE_MAP()
   afx_msg void OnChar(UINT i_uiChar, UINT i_uiRepCnt, UINT i_uiFlags);
   afx_msg void OnUpdate();

private:
   CString m_strPrevValue;
   int     m_iLastSel;
};


