// TextToolDoc.cpp : implementation of the CTextToolDoc class
//

#include "stdafx.h"
#include "TextTool.h"

#include "TextToolDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTextToolDoc

IMPLEMENT_DYNCREATE(CTextToolDoc, CDocument)

BEGIN_MESSAGE_MAP(CTextToolDoc, CDocument)
	//{{AFX_MSG_MAP(CTextToolDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTextToolDoc construction/destruction

CTextToolDoc::CTextToolDoc()
{
	// TODO: add one-time construction code here

}

CTextToolDoc::~CTextToolDoc()
{
}

BOOL CTextToolDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)

	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CTextToolDoc serialization

void CTextToolDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CTextToolDoc diagnostics

#ifdef _DEBUG
void CTextToolDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CTextToolDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CTextToolDoc commands
