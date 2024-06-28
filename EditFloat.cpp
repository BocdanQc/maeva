////////////////////////////////////////////////////////////////////////////////
// Filename:    EditFloat.cpp
//
// Description: Implementation file for the CEditFloat class derived from CEdit,
//              Allows float numbers only into the edit box.
//
// Author:      Daniel Emond, IFT and GLO Department, Laval University
////////////////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "EditFloat.h"

////////////////////////////////////////////////////////////////////////////////
// CEditFloat dialog
////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CEditFloat, CEdit)

CEditFloat::CEditFloat()
{
   m_iLastSel = 0;
}

CEditFloat::~CEditFloat()
{

}

BEGIN_MESSAGE_MAP(CEditFloat, CEdit)
   ON_WM_CHAR()
   ON_CONTROL_REFLECT(EN_UPDATE, OnUpdate)
END_MESSAGE_MAP()

void CEditFloat::OnChar(UINT i_uiChar, UINT i_uiRepCnt, UINT i_uiFlags)
{
   GetWindowText(m_strPrevValue);
   m_iLastSel = GetSel();
   CEdit::OnChar(i_uiChar, i_uiRepCnt, i_uiFlags);
}

void CEditFloat::OnUpdate()
{
   CString str;
   GetWindowText(str);
   errno = 0;
   LPTSTR endPtr;
   double doubleValue = strtod(str, &endPtr);
   if (*endPtr)
   {
      SetWindowText(m_strPrevValue);
      SetSel(m_iLastSel);
   }
}
