#pragma once
#ifdef WIN32
#define WIN_DROP_SOURCE 
#endif
#ifdef WIN_DROP_SOURCE
#include <afxole.h>         // MFC OLE classes

#include <shlobj.h>
#include <ole2.h>


#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif


#include "wx/evtloop.h"

// A dummy CWnd pointing to a wxWindow's HWND
class CDummyWindow : public CWnd
{
public:
    CDummyWindow(HWND hWnd);
    ~CDummyWindow(void);
};



struct SNIAfxDropSource : COleDataSource
{
    DECLARE_DYNAMIC(SNIAfxDropSource)
    // todo stop drag

    SNIAfxDropSource(std::function<void()> stop_handler) : stop_handler(stop_handler)
    {

    }

    std::function<void()> stop_handler;

    BOOL OnRenderFileData(
        LPFORMATETC lpFormatEtc, CFile* pFile)
    {
        // We only need to handle CFSTR_FILECONTENTS
        if (lpFormatEtc->cfFormat ==
            RegisterClipboardFormat(CFSTR_FILECONTENTS))
        {
            if (lpFormatEtc->tymed & (TYMED_FILE | TYMED_ISTREAM))
            {
                HGLOBAL hGlob = NULL;
                const int buffSize = 512;
                hGlob = GlobalAlloc(GMEM_FIXED, buffSize);
                if (hGlob)
                {
                    LPBYTE pBytes = (LPBYTE)GlobalLock(hGlob);
                    if (pBytes)
                    {
                        // lpFormatEtc->lindex can be used to identify
                        // the file that's being copied
                        /*memset(pBytes, (int)m_Files.GetAt(
                            lpFormatEtc->lindex)[0], buffSize);*/

                        pFile->Write(pBytes, buffSize);
                    }
                    GlobalUnlock(hGlob);
                }
                GlobalFree(hGlob);
                // Need to return TRUE to indicate success to Explorer

                if (stop_handler)
                {
                    stop_handler();
                }
                return TRUE;
            }
        }
        return COleDataSource::OnRenderFileData(
            lpFormatEtc, pFile);
    }

};
class SNFileTree;
struct SNWinDropFeedback : COleDropSource
{
    SNWinDropFeedback(SNFileTree* tree) : tree(tree)
    {

    }

    virtual SCODE QueryContinueDrag(
        BOOL bEscapePressed,
        DWORD dwKeyState);

    SNFileTree* tree;
};


#endif